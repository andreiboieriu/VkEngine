#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include "fmt/core.h"
#include <filesystem>
#include <memory>
#include <string_view>
#include <vk_loader.h>

#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_materials.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <vulkan/vulkan_core.h>

#include "stb_image.h"

// convert fastgltf opengl filter to vulkan
VkFilter extractFilter(fastgltf::Filter filter) {
    switch(filter) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
        return VK_FILTER_NEAREST;

    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter) {
    switch (filter) {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
        return VK_SAMPLER_MIPMAP_MODE_NEAREST;

    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(std::string_view filePath) {
    fmt::println("Loading GLTF: {}", filePath);

    std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
    // LoadedGLTF& file = *scene.get();


    // define gltf loading options
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

    // get file directory
    std::filesystem::path fileDirectory = std::filesystem::path(filePath).parent_path();

    // define needed fastgltf variables
    fastgltf::Parser parser{};
    fastgltf::Asset asset;

    fastgltf::GltfDataBuffer data;
    data.loadFromFile(filePath);

    // determine gltf type
    auto type = fastgltf::determineGltfFileType(&data);

    if (type == fastgltf::GltfType::glTF) {
        auto load = parser.loadGLTF(&data, fileDirectory, gltfOptions);

        if (!load) {
            fmt::println("Failed to load glTF: {}", fastgltf::to_underlying(load.error()));
            return {};
        }

        asset = std::move(load.get());
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data, fileDirectory, gltfOptions);

        if (!load) {
            fmt::println("Failed to load glTF: {}", fastgltf::to_underlying(load.error()));
            return {};
        }

        asset = std::move(load.get());
    } else {
        fmt::println("Failed to determine glTF container");
        return {};
    }

    // init scene descriptor allocator
    std::vector<DynamicDescriptorAllocator::PoolSizeRatio> poolRatios = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
    };

    scene->initDescriptorAllocator(asset.materials.size(), poolRatios);


    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<Node>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    std::vector<VkSampler> samplers;

    // load samplers
    for (auto& sampler : asset.samplers) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        samplerInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(VulkanEngine::get().getDevice(), &samplerInfo, nullptr, &newSampler);

        samplers.push_back(newSampler);
    }

    scene->setSamplers(samplers);

    // load all textures
    for (auto& image : asset.images) {
        std::optional<AllocatedImage> newImage = loadImage(asset, image);

        if (newImage.has_value()) {
            images.push_back(newImage.value());
            scene->addImage(image.name.c_str(), newImage.value());
        } else {
            images.push_back(VulkanEngine::get().getErrorTexture());
            fmt::println("gltf failed to load texture: {}", image.name);
        }
    }

    // create buffer to hold material data
    scene->initMaterialDataBuffer(asset.materials.size());

    int dataIndex = 0;
    // GLTFMetallicRoughness::MaterialConstants* sceneMaterialConstants

    for (auto& material : asset.materials) {
        std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
        materials.push_back(newMaterial);

        scene->addMaterial(material.name.c_str(), newMaterial);

        GLTFMetallicRoughness::MaterialConstants constants;
        constants.colorFactors.x = material.pbrData.baseColorFactor[0];
        constants.colorFactors.y = material.pbrData.baseColorFactor[1];
        constants.colorFactors.z = material.pbrData.baseColorFactor[2];
        constants.colorFactors.w = material.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = material.pbrData.metallicFactor;
        constants.metalRoughFactors.y = material.pbrData.roughnessFactor;

        // add material constants to buffer
        scene->addMaterialConstants(constants);

        MaterialPass passType = MaterialPass::Opaque;

        if (material.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        GLTFMetallicRoughness::MaterialResources materialResources;

        // default material textures
        materialResources.colorImage = VulkanEngine::get().getWhiteTexture();
        materialResources.colorSampler = VulkanEngine::get().getDefaultLinearSampler();
        materialResources.metalRoughImage = VulkanEngine::get().getWhiteTexture();
        materialResources.metalRoughSampler = VulkanEngine::get().getDefaultLinearSampler();

        // set uniform buffer for the material data
        materialResources.dataBuffer = scene->getMaterialDataBuffer();
        materialResources.dataBufferOffset = dataIndex * sizeof(GLTFMetallicRoughness::MaterialConstants);

        // get textures from gltf file
        if (material.pbrData.baseColorTexture.has_value()) {
            size_t image = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[image];
            materialResources.colorSampler = samplers[sampler];
        }

        newMaterial->materialInstance = VulkanEngine::get().getGLTFMRCreator().writeMaterial(VulkanEngine::get().getDevice(), passType, materialResources, scene->getDescriptorAllocator());

        dataIndex++;
    }

    // load meshes
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (auto& mesh : asset.meshes) {
        std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
        newMesh->name = mesh.name;

        meshes.push_back(newMesh);
        scene->addMesh(mesh.name.c_str(), newMesh);


        indices.clear();
        vertices.clear();

        for (auto&& p : mesh.primitives) {
            GeoSurface newSurface;
            newSurface.startIndex = (uint32_t)indices.size();
            newSurface.count = (uint32_t)asset.accessors[p.indicesAccessor.value()].count;

            size_t initialVtx = vertices.size();

            // load indexes
            {
                fastgltf::Accessor& indexAccessor = asset.accessors[p.indicesAccessor.value()];
                indices.reserve(indices.size() + indexAccessor.count);

                fastgltf::iterateAccessor<std::uint32_t>(asset, indexAccessor, 
                    [&](std::uint32_t index) {
                        indices.push_back(index + initialVtx);
                    });
            }

            // load vertex positions
            {
                fastgltf::Accessor& posAccessor = asset.accessors[p.findAttribute("POSITION")->second];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor,
                    [&](glm::vec3 v, size_t index) {
                        Vertex newVertex;
                        newVertex.position = v;
                        newVertex.normal = {1, 0, 0};
                        newVertex.color = glm::vec4(1.f);
                        newVertex.uvX = 0;
                        newVertex.uvY = 0;
                        vertices[initialVtx + index] = newVertex;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[(*normals).second],
                    [&](glm::vec3 v, size_t index) {
                        vertices[initialVtx + index].normal = v;
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[(*uv).second],
                [&](glm::vec2 v, size_t index) {
                    vertices[initialVtx + index].uvX = v.x;
                    vertices[initialVtx + index].uvY = v.y;
                });
            }

            // load vertex colors
            auto colors = p.findAttribute("COLOR_0");
            if (colors != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[(*colors).second],
                [&](glm::vec4 v, size_t index) {
                    vertices[initialVtx + index].color = v;
                });
            }

            if (p.materialIndex.has_value()) {
                newSurface.material = materials[p.materialIndex.value()];
            } else {
                newSurface.material = materials[0];
            }

            glm::vec3 minPos = vertices[initialVtx].position;
            glm::vec3 maxPos = vertices[initialVtx].position;

            for (int i = initialVtx; i < vertices.size(); i++) {
                minPos = glm::min(minPos, vertices[i].position);
                maxPos = glm::max(maxPos, vertices[i].position);
            }

            newSurface.bounds.origin = (maxPos + minPos) / 2.f;
            newSurface.bounds.extents = (maxPos - minPos) / 2.f;
            newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

            newMesh->surfaces.push_back(newSurface);
        }

        // constexpr bool OverrideColors = false;
        // if (OverrideColors) {
        //     for (Vertex& vertex : vertices) {
        //         vertex.color = glm::vec4(vertex.normal, 1.f);
        //     }
        // }

        newMesh->meshBuffers = VulkanEngine::get().uploadMesh(indices, vertices);
    }

    // load all nodes and their meshes
    for (auto& node : asset.nodes) {
        std::shared_ptr<Node> newNode;

        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<MeshNode>();
            std::static_pointer_cast<MeshNode>(newNode)->setMesh(meshes[node.meshIndex.value()]);
        } else {
            newNode = std::make_shared<Node>();
        }

        nodes.push_back(newNode);
        scene->addNode(node.name.c_str(), newNode);

        std::visit(fastgltf::visitor {
            [&](fastgltf::Node::TransformMatrix matrix) {
                memcpy(&newNode->getLocalTransform(), matrix.data(), sizeof(matrix));
            },
            [&](fastgltf::Node::TRS transform) {
                glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
                glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                glm::mat4 rm = glm::toMat4(rot);
                glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                newNode->setLocalTransform(tm * rm * sm);
            }
        },
        node.transform);
    }

    // run loop again to setup transform hierarchy
    for (int i = 0; i < asset.nodes.size(); i++) {
        fastgltf::Node& node = asset.nodes[i];
        std::shared_ptr<Node>& sceneNode = nodes[i];

        for (auto& child : node.children) {
            sceneNode->addChild(nodes[child]);
            nodes[child]->setParent(sceneNode);
        }
    }

    // find top nodes
    for (auto& node : nodes) {
        if (!node->hasParent()) {
            scene->addTopNode(node);
            node->refreshTransform(glm::mat4(1.f));
        }
    }

    return scene;
}

std::optional<AllocatedImage> loadImage(fastgltf::Asset& asset, fastgltf::Image& image) {
    AllocatedImage newImage{};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0 && "Offsets are not supported by STBI");
                assert(filePath.uri.isLocalPath() && "File path is not local");

                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());

                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

                if (data) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = VulkanEngine::get().createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                unsigned char* data = stbi_load_from_memory(vector.bytes.data(), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

                if (data) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = VulkanEngine::get().createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                    [](auto& arg) {},
                    [&](fastgltf::sources::Vector& vector) {
                        unsigned char* data = stbi_load_from_memory(vector.bytes.data() + bufferView.byteOffset, static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);

                        if (data) {
                            VkExtent3D imageSize;
                            imageSize.width = width;
                            imageSize.height = height;
                            imageSize.depth = 1;

                            newImage = VulkanEngine::get().createImage(data, imageSize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, true);

                            stbi_image_free(data);
                        }
                    }
                },
                buffer.data);
            }
        },
        image.data);

    if (newImage.image == VK_NULL_HANDLE) {
        return {};
    } else {
        return newImage;
    }
}

LoadedGLTF::~LoadedGLTF() {
    freeResources();
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, RenderContext& context) {
    for (auto& node : mTopNodes) {
        node->draw(topMatrix, context);
    }
}

void LoadedGLTF::initDescriptorAllocator(uint32_t initialSets, std::span<DynamicDescriptorAllocator::PoolSizeRatio> poolRatios) {
    mDescriptorAllocator.init(VulkanEngine::get().getDevice(), initialSets, poolRatios);
}

void LoadedGLTF::addSampler(VkSampler sampler) {
    mSamplers.push_back(sampler);
}

void LoadedGLTF::setSamplers(std::vector<VkSampler> samplers) {
    mSamplers = samplers;
}

void LoadedGLTF::initMaterialDataBuffer(size_t materialCount) {
    mMaterialDataBuffer = VulkanEngine::get().createBuffer(
        sizeof(GLTFMetallicRoughness::MaterialConstants) * materialCount,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

VkBuffer& LoadedGLTF::getMaterialDataBuffer() {
    return mMaterialDataBuffer.buffer;
}

DynamicDescriptorAllocator& LoadedGLTF::getDescriptorAllocator() {
    return mDescriptorAllocator;
}

void LoadedGLTF::addMaterial(std::string name, std::shared_ptr<GLTFMaterial> material) {
    mMaterials[name] = material;
}

void LoadedGLTF::addMaterialConstants(GLTFMetallicRoughness::MaterialConstants constants) {
    ((GLTFMetallicRoughness::MaterialConstants*)mMaterialDataBuffer.allocInfo.pMappedData)[mMaterialDataIndex++] = constants;
}

void LoadedGLTF::addNode(std::string name, std::shared_ptr<Node> node) {
    mNodes[name] = node;
}

void LoadedGLTF::addTopNode(std::shared_ptr<Node> topNode) {
    mTopNodes.push_back(topNode);
}

void LoadedGLTF::addMesh(std::string name, std::shared_ptr<MeshAsset> mesh) {
    mMeshes[name] = mesh;
}

void LoadedGLTF::addImage(std::string name, AllocatedImage image) {
    mImages[name] = image;
}

void LoadedGLTF::freeResources() {
    VkDevice device = VulkanEngine::get().getDevice();

    mDescriptorAllocator.destroyPools(device);
    VulkanEngine::get().destroyBuffer(mMaterialDataBuffer);

    for (auto& [k, v] : mMeshes) {
        VulkanEngine::get().destroyBuffer(v->meshBuffers.indexBuffer);
        VulkanEngine::get().destroyBuffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : mImages) {
        if (v.image == VulkanEngine::get().getErrorTexture().image) {
            continue;
        }

        VulkanEngine::get().destroyImage(v);
    }

    for (auto& sampler : mSamplers) {
        vkDestroySampler(device, sampler, nullptr);
    }
}
