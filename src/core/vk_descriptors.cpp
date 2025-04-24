#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"
#include "vk_engine.h"

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags shaderStages) {
    VkDescriptorSetLayoutBinding newBind{};
    newBind.binding = binding;
    newBind.descriptorCount = 1;
    newBind.descriptorType = type;
    newBind.stageFlags = shaderStages;

    mBindings.push_back(newBind);

    return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::clear() {
    mBindings.clear();

    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, void *pNext, VkDescriptorSetLayoutCreateFlags flags) {
    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.bindingCount = (uint32_t)mBindings.size();
    createInfo.pBindings = mBindings.data();
    createInfo.flags = flags;

    VkDescriptorSetLayout descriptorSet;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSet));

    return descriptorSet;
}

DescriptorWriter& DescriptorWriter::writeImage(int binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, VkDescriptorType type) {
    VkDescriptorImageInfo& imageInfo = mImageInfos.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = imageView,
        .imageLayout = layout
    });

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;

    mWrites.push_back(write);

    return *this;
}

DescriptorWriter& DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type) {
    VkDescriptorBufferInfo& bufferInfo = mBufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size
    });

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = binding;
    write.dstSet = VK_NULL_HANDLE;
    write.descriptorCount = 1;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;

    mWrites.push_back(write);

    return *this;
}

DescriptorWriter& DescriptorWriter::clear() {
    mImageInfos.clear();
    mWrites.clear();
    mBufferInfos.clear();

    return *this;
}

DescriptorWriter& DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set) {
    for (VkWriteDescriptorSet& write : mWrites) {
        write.dstSet = set;
    }

    vkUpdateDescriptorSets(device, (uint32_t)mWrites.size(), mWrites.data(), 0, nullptr);

    return *this;
}
