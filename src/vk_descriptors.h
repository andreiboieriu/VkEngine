#pragma once

#include <span>
#include <deque>
#include "volk.h"
#include "vk_types.h"

class DescriptorLayoutBuilder {
public:

    DescriptorLayoutBuilder& addBinding(uint32_t binding, VkDescriptorType type);
    DescriptorLayoutBuilder& clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

private:

    std::vector<VkDescriptorSetLayoutBinding> mBindings;
};

class DescriptorManager {

public:

    DescriptorManager(VkDescriptorSetLayout globalLayout, VkDescriptorSetLayout materialLayout);
    ~DescriptorManager();

    void bindDescriptorBuffers(VkCommandBuffer commandBuffer);
    VkDeviceSize createGlobalDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size);
    VkDeviceSize createMaterialDescriptor(const MaterialResources& resources);

private:

    void init();
    void createDescriptorBuffers();

    AllocatedBuffer mUniformDescriptorBuffer;
    VkDeviceSize mUniformCurrentOffset = 0;

    AllocatedBuffer mImageDescriptorBuffer;
    VkDeviceSize mImageCurrentOffset = 0;

    VkDescriptorSetLayout mGlobalLayout;
    VkDescriptorSetLayout mMaterialLayout;

    VkDeviceSize mGlobalLayoutSize;
    VkDeviceSize mMaterialLayoutSize;

    VkDeviceSize mGlobalLayoutOffset;
    VkDeviceSize mMaterialLayoutOffset;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties;
};

class DynamicDescriptorAllocator {
public:
    struct PoolSizeRatio {
        VkDescriptorType type;
        float ratio;
    };

    void init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios);
    void clearPools(VkDevice device);
    void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);

private:

    VkDescriptorPool getPool(VkDevice device);
    VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

    std::vector<PoolSizeRatio> mRatios;
    std::vector<VkDescriptorPool> mFullPools;
    std::vector<VkDescriptorPool> mAvailablePools;
    uint32_t mSetsPerPool;
    uint32_t mMaxSetsPerPool = 4092;
};

class DescriptorWriter {
public:

    DescriptorWriter& writeImage(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    DescriptorWriter& writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type);

    DescriptorWriter& clear();
    DescriptorWriter& updateSet(VkDevice device, VkDescriptorSet set);

    std::vector<VkWriteDescriptorSet> getWrites() {
        return mWrites;
    }

private:

    std::deque<VkDescriptorImageInfo> mImageInfos;
    std::deque<VkDescriptorBufferInfo> mBufferInfos;
    std::vector<VkWriteDescriptorSet> mWrites;
};
