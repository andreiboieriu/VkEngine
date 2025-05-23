#include "vk_loader.h"

#include "fastgltf/tools.hpp"
#include "vk_engine.h"
#include "pipeline_resource_manager.h"
#include "vk_types.h"
#include <glm/gtx/quaternion.hpp>

#include "volk.h"
#include "stb_image.h"
#include <fmt/core.h>
#include <filesystem>
#include <memory>
#include <string_view>

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

AllocatedImage LoadedGLTF::loadImage(fastgltf::Asset& asset, fastgltf::Image& image, VkFormat format, bool mipmapped) {
    std::string imageKey = std::to_string(&image - asset.images.data());

    if (mImages.contains(imageKey)) {
        return mImages[imageKey];
    }

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

                    newImage = mVkEngine.createImage(data, imageSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, mipmapped);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Array& vector) {
                unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

                if (data) {
                    VkExtent3D imageSize;
                    imageSize.width = width;
                    imageSize.height = height;
                    imageSize.depth = 1;

                    newImage = mVkEngine.createImage(data, imageSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, mipmapped);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];

                std::visit(fastgltf::visitor {
                    [](auto& arg) {},
                    [&](fastgltf::sources::Array& vector) {
                        unsigned char* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset), static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);

                        if (data) {
                            VkExtent3D imageSize;
                            imageSize.width = width;
                            imageSize.height = height;
                            imageSize.depth = 1;

                            newImage = mVkEngine.createImage(data, imageSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, mipmapped);

                            stbi_image_free(data);
                        }
                    }
                },
                buffer.data);
            }
        },
        image.data);

    if (newImage.image == VK_NULL_HANDLE) {
        return *mVkEngine.getAssetManager().getDefaultImage("error");
    } else {
        mImages[imageKey] = newImage;

        return newImage;
    }
}

void LoadedGLTF::load(std::filesystem::path filePath) {
    // define gltf loading options
    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember |
                                 fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadExternalBuffers;

    // create parser
    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_emissive_strength);

    // open gltf file
    auto gltfFile = fastgltf::MappedGltfFile::FromPath(filePath);

    if (!gltfFile) {
        fmt::println("Failed to open gltf file: {}", fastgltf::getErrorMessage(gltfFile.error()));
        return;
    }

    // parse gltf file
    auto assetExpected = parser.loadGltf(gltfFile.get(), filePath.parent_path(), gltfOptions);

    if (assetExpected.error() != fastgltf::Error::None) {
        fmt::println("Failed to load gltf: {}", fastgltf::getErrorMessage(assetExpected.error()));
        return;
    }

    fastgltf::Asset& asset = assetExpected.get();

    // temporal arrays for all the objects to use while creating the GLTF data
    std::vector<std::shared_ptr<MeshAsset>> meshes;
    std::vector<std::shared_ptr<GLTFNode>> nodes;
    std::vector<std::shared_ptr<GLTFMaterial>> materials;
    std::vector<VkSampler> samplers;

    // load samplers
    for (auto& sampler : asset.samplers) {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        samplerInfo.minLod = 0;

        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = mVkEngine.getMaxSamplerAnisotropy();

        samplerInfo.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
        samplerInfo.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        samplerInfo.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

        VkSampler newSampler;
        vkCreateSampler(mVkEngine.getDevice(), &samplerInfo, nullptr, &newSampler);

        samplers.push_back(newSampler);
    }

    mSamplers = samplers;

    // create buffer to hold material data
    initMaterialDataBuffer(asset.materials.size());

    int dataIndex = 0;

    for (auto& material : asset.materials) {
        std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
        materials.push_back(newMaterial);

        mMaterials[material.name.c_str()] = newMaterial;

        MaterialConstants constants{};
        constants.colorFactors.x = material.pbrData.baseColorFactor[0];
        constants.colorFactors.y = material.pbrData.baseColorFactor[1];
        constants.colorFactors.z = material.pbrData.baseColorFactor[2];
        constants.colorFactors.w = material.pbrData.baseColorFactor[3];

        constants.metalRoughFactors.x = material.pbrData.metallicFactor;
        constants.metalRoughFactors.y = material.pbrData.roughnessFactor;

        MaterialPass passType = MaterialPass::Opaque;

        if (material.alphaMode == fastgltf::AlphaMode::Blend) {
            passType = MaterialPass::Transparent;
        }

        if (material.doubleSided) {
            if (passType == MaterialPass::Opaque) {
                passType = MaterialPass::OpaqueDoubleSided;
            } else {
                passType = MaterialPass::TransparentDoubleSided;
            }
        }

        MaterialResources materialResources;

        // default material textures
        materialResources.colorImage = *mVkEngine.getAssetManager().getDefaultImage("white");
        materialResources.colorSampler = *mVkEngine.getAssetManager().getSampler("linear");
        materialResources.metalRoughImage = *mVkEngine.getAssetManager().getDefaultImage("white");
        materialResources.metalRoughSampler = *mVkEngine.getAssetManager().getSampler("linear");
        materialResources.normalImage = *mVkEngine.getAssetManager().getDefaultImage("normal");
        materialResources.normalSampler = *mVkEngine.getAssetManager().getSampler("linear");
        materialResources.emissiveImage = *mVkEngine.getAssetManager().getDefaultImage("black");
        materialResources.emissiveSampler = *mVkEngine.getAssetManager().getSampler("linear");
        materialResources.occlusionImage = *mVkEngine.getAssetManager().getDefaultImage("white");
        materialResources.occlusionSampler = *mVkEngine.getAssetManager().getSampler("linear");

        // set uniform buffer for the material data
        materialResources.dataBuffer = mMaterialDataBuffer.buffer;
        materialResources.dataBufferOffset = dataIndex * sizeof(MaterialConstants);
        materialResources.dataBufferAddress = mMaterialDataBuffer.deviceAddress;

        // get textures from gltf file
        if (material.pbrData.baseColorTexture.has_value()) {
            size_t image = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
            fmt::println("Base color index {}", image);

            auto sampler = asset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex;

            if (sampler.has_value()) {
                materialResources.colorSampler = samplers[sampler.value()];
            }

            materialResources.colorImage = loadImage(asset, asset.images[image], VK_FORMAT_R8G8B8A8_SRGB, true);
        }

        if (material.pbrData.metallicRoughnessTexture.has_value()) {
            size_t image = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
            auto sampler = asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].samplerIndex;
            fmt::println("Metal Rough index {}", image);

            if (sampler.has_value()) {
                materialResources.metalRoughSampler = samplers[sampler.value()];
            }

            materialResources.metalRoughImage = loadImage(asset, asset.images[image], VK_FORMAT_R8G8B8A8_UNORM, true);
        }

        if (material.normalTexture.has_value()) {
            size_t image = asset.textures[material.normalTexture.value().textureIndex].imageIndex.value();
            auto sampler = asset.textures[material.normalTexture.value().textureIndex].samplerIndex;

            if (sampler.has_value()) {
                materialResources.normalSampler = samplers[sampler.value()];
            }

            materialResources.normalImage = loadImage(asset, asset.images[image], VK_FORMAT_R8G8B8A8_UNORM, true);

            constants.normalScale = material.normalTexture.value().scale;
        }

        if (material.emissiveTexture.has_value()) {
            size_t image = asset.textures[material.emissiveTexture.value().textureIndex].imageIndex.value();
            auto sampler = asset.textures[material.emissiveTexture.value().textureIndex].samplerIndex;

            if (sampler.has_value()) {
                materialResources.emissiveSampler = samplers[sampler.value()];
            }

            materialResources.emissiveImage = loadImage(asset, asset.images[image], VK_FORMAT_R8G8B8A8_SRGB, true);

            constants.emissiveFactors.x = material.emissiveFactor[0];
            constants.emissiveFactors.y = material.emissiveFactor[1];
            constants.emissiveFactors.z = material.emissiveFactor[2];

            constants.emissiveStrength = material.emissiveStrength;
        }

        if (material.occlusionTexture.has_value()) {
            size_t image = asset.textures[material.occlusionTexture.value().textureIndex].imageIndex.value();
            auto sampler = asset.textures[material.occlusionTexture.value().textureIndex].samplerIndex;

            if (sampler.has_value()) {
                materialResources.occlusionSampler = samplers[sampler.value()];
            }

            materialResources.occlusionImage = loadImage(asset, asset.images[image], VK_FORMAT_R8G8B8A8_UNORM, true);
            constants.occlusionStrength = material.occlusionTexture.value().strength;
        }


        // add material constants to buffer
        ((MaterialConstants*)mMaterialDataBuffer.allocInfo.pMappedData)[dataIndex] = constants;

        newMaterial->materialInstance = mVkEngine.getPipelineResourceManager().writeMaterial(mVkEngine.getDevice(), passType, materialResources);

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
                fastgltf::Accessor& posAccessor = asset.accessors[p.findAttribute("POSITION")->accessorIndex];
                vertices.resize(vertices.size() + posAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, posAccessor,
                    [&](fastgltf::math::fvec3 v, size_t index) {
                        Vertex newVertex;
                        newVertex.position = glm::vec3(v.x(), v.y(), v.z());
                        newVertex.normal = {1, 0, 0};
                        newVertex.uvX = 0;
                        newVertex.uvY = 0;
                        vertices[initialVtx + index] = newVertex;
                    });
            }

            // load vertex normals
            auto normals = p.findAttribute("NORMAL");
            if (normals != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, asset.accessors[normals->accessorIndex],
                    [&](fastgltf::math::fvec3 v, size_t index) {
                        vertices[initialVtx + index].normal = glm::vec3(v.x(), v.y(), v.z());
                    });
            }

            // load UVs
            auto uv = p.findAttribute("TEXCOORD_0");
            if (uv != p.attributes.end()) {
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, asset.accessors[uv->accessorIndex],
                [&](fastgltf::math::fvec2 v, size_t index) {
                    vertices[initialVtx + index].uvX = v.x();
                    vertices[initialVtx + index].uvY = v.y();
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

        newMesh->meshBuffers = mVkEngine.uploadMesh(indices, vertices);
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

        auto transformMatrix = fastgltf::getTransformMatrix(node);

        memcpy(&newNode->getLocalTransform(), transformMatrix.data(), transformMatrix.size_bytes());
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

LoadedGLTF::LoadedGLTF(std::filesystem::path filePath, VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    load(filePath);
}

LoadedGLTF::~LoadedGLTF() {
    freeResources();
}

void LoadedGLTF::draw(const glm::mat4& transform, RenderContext& context) {
    for (auto& node : mTopNodes) {
        node->draw(transform, context);
    }
}

void LoadedGLTF::initMaterialDataBuffer(size_t materialCount) {
    mMaterialDataBuffer = mVkEngine.createBuffer(
        sizeof(MaterialConstants) * materialCount,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

void LoadedGLTF::freeResources() {
    VkDevice device = mVkEngine.getDevice();

    mVkEngine.destroyBuffer(mMaterialDataBuffer);

    for (auto& [k, v] : mMeshes) {
        mVkEngine.destroyBuffer(v->meshBuffers.indexBuffer);
        mVkEngine.destroyBuffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : mImages) {
        if (v.image == mVkEngine.getAssetManager().getDefaultImage("error")->image) {
            continue;
        }

        mVkEngine.destroyImage(v);
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
            GLTFRenderObject object;
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
