#include "vk_descriptors.h"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newBind{};
    newBind.binding = binding;
    newBind.descriptorCount = 1;
    newBind.descriptorType = type;

    mBindings.push_back(newBind);
}

void DescriptorLayoutBuilder::clear() {
    mBindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void *pNext, VkDescriptorSetLayoutCreateFlags flags) {
    for (auto& binding : mBindings) {
        binding.stageFlags |= shaderStages;
    }

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

void DescriptorAllocator::initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;

    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize {
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * maxSets)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &mDescriptorPool));
}

void DescriptorAllocator::clearDescriptors(VkDevice device) {
    vkResetDescriptorPool(device, mDescriptorPool, 0);
}

void DescriptorAllocator::destroyPool(VkDevice device) {
    vkDestroyDescriptorPool(device, mDescriptorPool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = nullptr;
    allocInfo.descriptorPool = mDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    return descriptorSet;
}
