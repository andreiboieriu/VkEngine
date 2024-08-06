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

// ------------ DESCRIPTOR MANAGER -------------------

DescriptorManager::DescriptorManager(VkDescriptorSetLayout globalLayout, VkDescriptorSetLayout materialLayout) : mGlobalLayout(globalLayout), mMaterialLayout(materialLayout) {
    createDescriptorBuffers();
}

DescriptorManager::~DescriptorManager() {
}

void DescriptorManager::init() {

}

void DescriptorManager::createDescriptorBuffers() {
    VkDevice device = VulkanEngine::get().getDevice();

    // get set layout descriptor sizes
    vkGetDescriptorSetLayoutSizeEXT(device, mGlobalLayout, &mGlobalLayoutSize);
    vkGetDescriptorSetLayoutSizeEXT(device, mMaterialLayout, &mMaterialLayoutSize);

    // adjust set layout size to satisfy alignment requirements
    mDescriptorBufferProperties = VulkanEngine::get().getDescriptorBufferProperties();

    mGlobalLayoutSize = alignedSize(mGlobalLayoutSize, mDescriptorBufferProperties.descriptorBufferOffsetAlignment);
    mMaterialLayoutSize = alignedSize(mMaterialLayoutSize, mDescriptorBufferProperties.descriptorBufferOffsetAlignment);

    // get descriptor binding offsets
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, mGlobalLayout, 0u, &mGlobalLayoutOffset);
    vkGetDescriptorSetLayoutBindingOffsetEXT(device, mMaterialLayout, 0u, &mMaterialLayoutOffset);


    // create descriptor buffer
    mDescriptorBuffer = VulkanEngine::get().createBuffer(
        mMaterialLayoutSize * 10000,
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

void DescriptorManager::bindDescriptorBuffers(VkCommandBuffer commandBuffer) {
    VkDescriptorBufferBindingInfoEXT bindingInfo;

    bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindingInfo.address = mDescriptorBuffer.deviceAddress;
    bindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    bindingInfo.pNext = nullptr;

    vkCmdBindDescriptorBuffersEXT(commandBuffer, 1, &bindingInfo);
}
 
VkDeviceSize DescriptorManager::createGlobalDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size) {
    // create scene data descriptor
    VkDescriptorAddressInfoEXT addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    addressInfo.address = bufferAddress;
    addressInfo.range = size;
    addressInfo.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT descriptorInfo{};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorInfo.data.pUniformBuffer = &addressInfo;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &descriptorInfo,
        mDescriptorBufferProperties.uniformBufferDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mGlobalLayoutOffset
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += mGlobalLayoutSize;

    return retOffset;
}

VkDeviceSize DescriptorManager::createMaterialDescriptor(const MaterialResources& resources) {
    // create color texture descriptor
    VkDescriptorImageInfo imageDescriptor{};

    imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageDescriptor.imageView = resources.colorImage.imageView;
    imageDescriptor.sampler = resources.colorSampler;

    VkDescriptorGetInfoEXT imageDescriptorInfo{};
    imageDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    imageDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageDescriptorInfo.data.pCombinedImageSampler = &imageDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &imageDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset
    );

    // create metal rough texture descriptor
    VkDescriptorImageInfo metalDescriptor{};

    metalDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    metalDescriptor.imageView = resources.metalRoughImage.imageView;
    metalDescriptor.sampler = resources.metalRoughSampler;

    VkDescriptorGetInfoEXT metalDescriptorInfo{};
    metalDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    metalDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    metalDescriptorInfo.data.pCombinedImageSampler = &metalDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &metalDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset + mDescriptorBufferProperties.combinedImageSamplerDescriptorSize
    );

    // create material constants descriptor
    VkDescriptorAddressInfoEXT addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    addressInfo.address = resources.dataBufferAddress + resources.dataBufferOffset;
    addressInfo.range = sizeof(MaterialConstants);
    addressInfo.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT descriptorInfo{};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorInfo.data.pUniformBuffer = &addressInfo;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &descriptorInfo,
        mDescriptorBufferProperties.uniformBufferDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset + mDescriptorBufferProperties.combinedImageSamplerDescriptorSize + mDescriptorBufferProperties.combinedImageSamplerDescriptorSize
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += mMaterialLayoutSize;

    return retOffset;
}

void DescriptorManager::freeResources() {
    VulkanEngine::get().destroyBuffer(mDescriptorBuffer);
}


