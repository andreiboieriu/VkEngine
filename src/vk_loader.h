#pragma once

#include "fastgltf/types.hpp"
#include "vk_descriptors.h"
#include "vk_materials.h"
#include "vk_scene.h"
#include <memory>
#include <string>
#include <vk_types.h>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

class LoadedGLTF : public IRenderable {
public:
    ~LoadedGLTF();

    virtual void draw(const glm::mat4& topMatrix, RenderContext& context);

    void initDescriptorAllocator(uint32_t initialSets, std::span<DynamicDescriptorAllocator::PoolSizeRatio> poolRatios);

    void addSampler(VkSampler sampler);

    void setSamplers(std::vector<VkSampler> samplers);
    void initMaterialDataBuffer(size_t materialCount);
    VkBuffer& getMaterialDataBuffer();

    DynamicDescriptorAllocator& getDescriptorAllocator();

    void addMaterial(std::string name, std::shared_ptr<GLTFMaterial> material);

    void addMaterialConstants(GLTFMetallicRoughness::MaterialConstants constants);

    void addNode(std::string name, std::shared_ptr<Node> node);

    void addTopNode(std::shared_ptr<Node> topNode);

    void addMesh(std::string name, std::shared_ptr<MeshAsset> mesh);

    void addImage(std::string name, AllocatedImage image);

    std::vector<std::shared_ptr<Node>>& getTopNodes() {
        return mTopNodes;
    }

private:
    void freeResources();

    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> mMeshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> mNodes;
    std::unordered_map<std::string, AllocatedImage> mImages;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> mMaterials;

    std::vector<std::shared_ptr<Node>> mTopNodes;

    std::vector<VkSampler> mSamplers;
    DynamicDescriptorAllocator mDescriptorAllocator;

    AllocatedBuffer mMaterialDataBuffer;
    int mMaterialDataIndex = 0;
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(std::string_view filePath);
std::optional<AllocatedImage> loadImage(fastgltf::Asset& asset, fastgltf::Image& image);
