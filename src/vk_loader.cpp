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
#include "volk.h"
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

void LoadedGLTF::load(std::string_view filePath) {
    fmt::println("Loading GLTF: {}", filePath);

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
            return;
        }

        asset = std::move(load.get());
    } else if (type == fastgltf::GltfType::GLB) {
        auto load = parser.loadBinaryGLTF(&data, fileDirectory, gltfOptions);

        if (!load) {
            fmt::println("Failed to load binary glTF: {}", fastgltf::to_underlying(load.error()));
            return;
        }

        asset = std::move(load.get());
    } else {
        fmt::println("Failed to determine glTF container");
        return;
    }

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<GLTFNode>> nodes;
    std::vector<AllocatedImage> images;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    std::vector<VkSampler> samplers;

    // load samplers
    for (auto& sampler : asset.samplers) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = VulkanEngine::get().getMaxSamplerAnisotropy();

        samplerInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        samplerInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(VulkanEngine::get().getDevice(), &samplerInfo, nullptr, &newSampler);

        samplers.push_back(newSampler);
    }

    mSamplers = samplers;

    // load all textures
    for (auto& image : asset.images) {
        std::optional<AllocatedImage> newImage = loadImage(asset, image);

        if (newImage.has_value()) {
            if (mImages.contains(image.name.c_str())) {
                VulkanEngine::get().destroyImage(newImage.value());
                images.push_back(mImages[image.name.c_str()]);
                continue;
            }

            images.push_back(newImage.value());
            mImages[image.name.c_str()] = newImage.value();
        } else {
            images.push_back(VulkanEngine::get().getErrorTexture());
            fmt::println("gltf failed to load texture: {}", image.name);
        }
    }

    // create buffer to hold material data
    initMaterialDataBuffer(asset.materials.size());

    int dataIndex = 0;

    for (auto& material : asset.materials) {
        std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
        materials.push_back(newMaterial);

        mMaterials[material.name.c_str()] = newMaterial;

        MaterialConstants constants;
        constants.colorFactors.x = material.pbrData.baseColorFactor[0];
        constants.colorFactors.y = material.pbrData.baseColorFactor[1];
        constants.colorFactors.z = material.pbrData.baseColorFactor[2];
        constants.colorFactors.w = material.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = material.pbrData.metallicFactor;
        constants.metalRoughFactors.y = material.pbrData.roughnessFactor;

        // add material constants to buffer
        ((MaterialConstants*)mMaterialDataBuffer.allocInfo.pMappedData)[dataIndex] = constants;

        MaterialPass passType = MaterialPass::Opaque;

        if (material.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        MaterialResources materialResources;

        // default material textures
        materialResources.colorImage = VulkanEngine::get().getWhiteTexture();
        materialResources.colorSampler = VulkanEngine::get().getDefaultLinearSampler();
        materialResources.metalRoughImage = VulkanEngine::get().getWhiteTexture();
        materialResources.metalRoughSampler = VulkanEngine::get().getDefaultLinearSampler();

        // set uniform buffer for the material data
        materialResources.dataBuffer = mMaterialDataBuffer.buffer;
        materialResources.dataBufferOffset = dataIndex * sizeof(MaterialConstants);
        materialResources.dataBufferAddress = mMaterialDataBuffer.deviceAddress;

        if (material.specular && material.specular->specularTexture.has_value()) {
            fmt::println("found spec texture");
        }

        // get textures from gltf file
        if (material.pbrData.baseColorTexture.has_value()) {
            size_t image = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

            materialResources.colorImage = images[image];
            materialResources.colorSampler = samplers[sampler];
        }

        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            size_t image = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            size_t sampler = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex.value();

            materialResources.metalRoughImage = images[image];
            materialResources.metalRoughSampler = samplers[sampler];
        }

        newMaterial->materialInstance = VulkanEngine::get().getMaterialManager().writeMaterial(VulkanEngine::get().getDevice(), passType, materialResources);

        dataIndex++;
    }

    // load meshes
    std::vector<uint32_t> indices;
    std::vector<Vertex> vertices;

    for (auto& mesh : asset.meshes) {
        if (mMeshes.contains(mesh.name.c_str())) {
            meshes.push_back(mMeshes[mesh.name.c_str()]);
            continue;
        }

        std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
        newMesh->name = mesh.name;

        meshes.push_back(newMesh);
        mMeshes[mesh.name.c_str()] = newMesh;

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
        std::shared_ptr<GLTFNode> newNode;

        if (node.meshIndex.has_value()) {
            newNode = std::make_shared<GLTFNode>(meshes[node.meshIndex.value()]);
        } else {
            newNode = std::make_shared<GLTFNode>();
        }

        nodes.push_back(newNode);
        mNodes[node.name.c_str()] = newNode;

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
        std::shared_ptr<GLTFNode>& sceneNode = nodes[i];

        for (auto& child : node.children) {
            sceneNode->addChild(nodes[child]);
            nodes[child]->setParent(sceneNode);
        }
    }

    // find top nodes
    for (auto& node : nodes) {
        if (!node->hasParent()) {
            mTopNodes.push_back(node);
            node->propagateTransform();
        }
    }
}

LoadedGLTF::LoadedGLTF(std::string_view filePath) {
    load(filePath);
}

LoadedGLTF::~LoadedGLTF() {
    // freeResources();
}

void LoadedGLTF::draw(const glm::mat4& transform, RenderContext& context) {
    for (auto& node : mTopNodes) {
        node->draw(transform, context);
    }
}

void LoadedGLTF::initMaterialDataBuffer(size_t materialCount) {
    mMaterialDataBuffer = VulkanEngine::get().createBuffer(
        sizeof(MaterialConstants) * materialCount,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

void LoadedGLTF::freeResources() {
    VkDevice device = VulkanEngine::get().getDevice();

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

GLTFNode::GLTFNode(std::shared_ptr<MeshAsset> mesh) {
    mMesh = mesh;
}

void GLTFNode::draw(const glm::mat4& transform, RenderContext& context) {
    if (mMesh != nullptr) {
        glm::mat4 nodeTransform = transform * mGlobalTransform;

        for (auto& surface : mMesh->surfaces) {
            RenderObject object;
            object.indexCount = surface.count;
            object.firstIndex = surface.startIndex;
            object.indexBuffer = mMesh->meshBuffers.indexBuffer.buffer;
            object.material = &surface.material->materialInstance;
            object.bounds = surface.bounds;
            object.transform = nodeTransform;
            object.vertexBufferAddress = mMesh->meshBuffers.vertexBufferAddress;

            if (surface.material->materialInstance.passType == MaterialPass::Opaque) {
                context.opaqueObjects.push_back(object);
            } else {
                context.transparentObjects.push_back(object);
            }
        }
    }

    for (auto& node : mChildren) {
        node->draw(transform, context);
    }
}

void GLTFNode::addChild(std::shared_ptr<GLTFNode> child) {
    mChildren.push_back(child);
}

bool GLTFNode::hasParent() {
    return mParent.lock() != nullptr;
}

void GLTFNode::setParent(std::weak_ptr<GLTFNode> parent) {
    mParent = parent;
}

void GLTFNode::propagateTransform(const glm::mat4& transform) {
    mGlobalTransform = transform * mLocalTransform;

    for (auto& node : mChildren) {
        node->propagateTransform(mGlobalTransform);
    }
}

void GLTFNode::setLocalTransform(glm::mat4 transform) {
    mLocalTransform = transform;
}

void GLTFNode::setGlobalTransform(glm::mat4 transform) {
    mGlobalTransform = transform;
}

glm::mat4& GLTFNode::getLocalTransform() {
    return mLocalTransform;
}

glm::mat4& GLTFNode::getGlobalTransform() {
    return mGlobalTransform;
}
