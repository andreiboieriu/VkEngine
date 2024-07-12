#pragma once

#include "vk_types.h"

class DescriptorLayoutBuilder {
public:

    void addBinding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

private:

    std::vector<VkDescriptorSetLayoutBinding> mBindings;
};

class DescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void clearDescriptors(VkDevice device);
    void destroyPool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:

    VkDescriptorPool mDescriptorPool;

};
