#pragma once

#include <span>
#include <deque>
#include "volk.h"
#include "vk_types.h"

// forward reference
class VulkanEngine;

class DescriptorLayoutBuilder {
public:

    DescriptorLayoutBuilder& addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages);
    DescriptorLayoutBuilder& clear();
    VkDescriptorSetLayout build(VkDevice device, void *pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

private:

    std::vector<VkDescriptorSetLayoutBinding> mBindings;
};

class DescriptorManager {

public:

    DescriptorManager(VkDescriptorSetLayout sceneLayout, VkDescriptorSetLayout materialLayout, VulkanEngine& vkEngine);
    ~DescriptorManager();

    void bindDescriptorBuffers(VkCommandBuffer commandBuffer);
    VkDeviceSize createSceneDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size, VkImageView irradianceMapView, VkImageView prefilteredEnvMapView, VkImageView brdflutView);
    VkDeviceSize createMaterialDescriptor(const MaterialResources& resources);

    void freeResources();

private:

    void init();
    void createDescriptorBuffers();

    VulkanEngine& mVkEngine;

    AllocatedBuffer mDescriptorBuffer;
    VkDeviceSize mCurrentOffset = 0;

    VkDescriptorSetLayout mGlobalLayout;
    VkDescriptorSetLayout mMaterialLayout;

    VkDeviceSize mGlobalLayoutSize;
    VkDeviceSize mMaterialLayoutSize;

    VkDeviceSize mGlobalLayoutOffset[4];
    VkDeviceSize mMaterialLayoutOffset[6];

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
