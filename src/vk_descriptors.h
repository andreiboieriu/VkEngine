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

    void freeResources();

private:

    void init();
    void createDescriptorBuffers();

    AllocatedBuffer mDescriptorBuffer;
    VkDeviceSize mCurrentOffset = 0;

    VkDescriptorSetLayout mGlobalLayout;
    VkDescriptorSetLayout mMaterialLayout;

    VkDeviceSize mGlobalLayoutSize;
    VkDeviceSize mMaterialLayoutSize;

    VkDeviceSize mGlobalLayoutOffset;
    VkDeviceSize mMaterialLayoutOffset;

    VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties;
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
