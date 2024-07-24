#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"

class GLTFMetallicRoughness {
public:

    void buildPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void clearResources(VkDevice device);

    VkDescriptorSetLayout getGlobalDescriptorSetLayout() {
        return mGlobalDescriptorLayout;
    }

    VkDescriptorSetLayout getMaterialDescriptorSetLayout() {
        return mMaterialDescriptorLayout;
    }

    MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorManager& descriptorManager);

private:

    DescriptorWriter mDescriptorWriter;
    MaterialPipeline mOpaquePipeline;
    MaterialPipeline mTransparentPipeline;

    VkDescriptorSetLayout mMaterialDescriptorLayout;
    VkDescriptorSetLayout mGlobalDescriptorLayout;

};
