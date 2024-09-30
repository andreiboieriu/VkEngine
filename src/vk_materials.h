#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"

class MaterialManager {
public:

    MaterialManager();
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


    DescriptorWriter mDescriptorWriter;
    MaterialPipeline mOpaquePipelineCulled;
    MaterialPipeline mOpaquePipeline;
    MaterialPipeline mTransparentPipelineCulled;
    MaterialPipeline mTransparentPipeline;

    VkDescriptorSetLayout mMaterialDescriptorLayout;
    VkDescriptorSetLayout mSceneDescriptorLayout;

};
