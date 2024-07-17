#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include <vulkan/vulkan_core.h>

class GLTFMetallicRoughness {
public:

    struct MaterialConstants {
        glm::vec4 colorFactors;
        glm::vec4 metalRoughFactors;

        // padding
        glm::vec4 extra[14];
    };

    struct MaterialResources {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    void buildPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
    void clearResources(VkDevice device);

    MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DynamicDescriptorAllocator& descriptorAllocator);

private:

    DescriptorWriter mDescriptorWriter;
    MaterialPipeline mOpaquePipeline;
    MaterialPipeline mTransparentPipeline;

    VkDescriptorSetLayout mMaterialLayout;
    VkDescriptorSetLayout mGpuSceneDataLayout;

};
