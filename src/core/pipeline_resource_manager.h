#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"

// forward reference
class VulkanEngine;

class PipelineResourceManager {
public:
    enum class PipelineType : uint8_t {
        PBR_OPAQUE,
        PBR_TRANSPARENT,
        PBR_OPAQUE_DOUBLE_SIDED,
        PBR_TRANSPARENT_DOUBLE_SIDED,
        SPRITE,
        PHONG,
        SKYBOX,
        EQUI_TO_CUBE,
        ENV_TO_IRR,
        PREFILTER_ENV
    };

    enum class DescriptorSetLayoutType : uint8_t {
        PBR,
        PHONG,
        SPRITE,
        PBR_SCENE_DATA,
        SKYBOX
    };

    PipelineResourceManager(VulkanEngine& vkEngine);
    ~PipelineResourceManager();

    VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayoutType type) {
        return mDescriptorSetLayouts[type].layout;
    }

    Pipeline *getPipeline(PipelineType type) {
        return &mPipelines[type];
    }

    void bindDescriptorBuffers(VkCommandBuffer commandBuffer);
    VkDeviceSize createMaterialDescriptor(const MaterialResources& resources);
    VkDeviceSize createSceneDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size, VkImageView irradianceMapView, VkImageView prefilteredEnvMapView, VkImageView brdflutView);
    MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources);

private:
    struct DescriptorSetLayoutInfo {
        VkDescriptorSetLayout layout;
        VkDeviceSize size;
        VkDeviceSize alignedSize;
        std::vector<VkDeviceSize> bindingOffsets;
    };

    void init(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);

    void createDescriptorBuffers(VkDevice device);
    void createDescriptorSetLayouts(VkDevice device);

    void buildPBRPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void buildPhongPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void buildSpritePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void buildSkyboxPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);

    void freeResources();

    VulkanEngine& mVkEngine;

    DescriptorWriter mDescriptorWriter;

    std::unordered_map<PipelineType, Pipeline> mPipelines;
    std::unordered_map<DescriptorSetLayoutType, DescriptorSetLayoutInfo> mDescriptorSetLayouts;

    AllocatedBuffer mDescriptorBuffer;
    VkDeviceSize mCurrentOffset = 0;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties;
};
