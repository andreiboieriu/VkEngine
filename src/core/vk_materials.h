#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"

// forward reference
class VulkanEngine;

class MaterialManager {
public:

    MaterialManager(VulkanEngine& vkEngine);
    ~MaterialManager();

    VkDescriptorSetLayout getSceneDescriptorSetLayout() {
        return mSceneDescriptorLayout;
    }

    VkDescriptorSetLayout getMaterialDescriptorSetLayout() {
        return mMaterialDescriptorLayout;
    }

    MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources);

private:
    void buildPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void freeResources();

    VulkanEngine& mVkEngine;

    DescriptorWriter mDescriptorWriter;
    MaterialPipeline mOpaquePipelineCulled;
    MaterialPipeline mOpaquePipeline;
    MaterialPipeline mTransparentPipelineCulled;
    MaterialPipeline mTransparentPipeline;

    VkDescriptorSetLayout mMaterialDescriptorLayout;
    VkDescriptorSetLayout mSceneDescriptorLayout;
};
