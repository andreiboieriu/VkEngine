﻿#include "vk_descriptors.h"
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
    freeResources();
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
    for (unsigned int i = 0; i < 4; i++)
        vkGetDescriptorSetLayoutBindingOffsetEXT(device, mGlobalLayout, i, &mGlobalLayoutOffset[i]);
    
    for (unsigned int i = 0; i < 6; i++)
        vkGetDescriptorSetLayoutBindingOffsetEXT(device, mMaterialLayout, i, &mMaterialLayoutOffset[i]);


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
 
VkDeviceSize DescriptorManager::createSceneDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size, VkImageView irradianceMapView, VkImageView prefilteredEnvMapView, VkImageView brdflutView) {
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
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mGlobalLayoutOffset[0]
    );

    // create irradiance map descriptor
    VkDescriptorImageInfo imageDescriptor{};

    imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageDescriptor.imageView = irradianceMapView;
    imageDescriptor.sampler = VulkanEngine::get().getDefaultLinearSampler();

    VkDescriptorGetInfoEXT imageDescriptorInfo{};
    imageDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    imageDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageDescriptorInfo.data.pCombinedImageSampler = &imageDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &imageDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mGlobalLayoutOffset[1]
    );

    // create prefiltered env map descriptor
    VkDescriptorImageInfo prefilteredDescriptor{};

    prefilteredDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    prefilteredDescriptor.imageView = prefilteredEnvMapView;
    prefilteredDescriptor.sampler = VulkanEngine::get().getDefaultLinearSampler();

    VkDescriptorGetInfoEXT prefilteredDescriptorInfo{};
    prefilteredDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    prefilteredDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    prefilteredDescriptorInfo.data.pCombinedImageSampler = &prefilteredDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &prefilteredDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mGlobalLayoutOffset[2]
    );

    // create brdf lut descriptor
    VkDescriptorImageInfo brdflutDescriptor{};

    brdflutDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdflutDescriptor.imageView = brdflutView;
    brdflutDescriptor.sampler = VulkanEngine::get().getDefaultLinearSampler();

    VkDescriptorGetInfoEXT brdflutDescriptorInfo{};
    brdflutDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    brdflutDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brdflutDescriptorInfo.data.pCombinedImageSampler = &brdflutDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &brdflutDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mGlobalLayoutOffset[3]
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += mGlobalLayoutSize;
    fmt::println("created scene descriptor");

    if (brdflutView == prefilteredEnvMapView || brdflutView == irradianceMapView) {
        fmt::println("sug pula gratis");
    }

    return retOffset;
}

VkDeviceSize DescriptorManager::createMaterialDescriptor(const MaterialResources& resources) {
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
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[0]
    );

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
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[1]
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
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[2]
    );

    // create normal texture descriptor
    VkDescriptorImageInfo normalDescriptor{};

    normalDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normalDescriptor.imageView = resources.normalImage.imageView;
    normalDescriptor.sampler = resources.normalSampler;

    VkDescriptorGetInfoEXT normalDescriptorInfo{};
    normalDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    normalDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalDescriptorInfo.data.pCombinedImageSampler = &normalDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &normalDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[3]
    );

    // create emissive texture descriptor
    VkDescriptorImageInfo emissiveDescriptor{};

    emissiveDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    emissiveDescriptor.imageView = resources.emissiveImage.imageView;
    emissiveDescriptor.sampler = resources.emissiveSampler;

    VkDescriptorGetInfoEXT emissiveDescriptorInfo{};
    emissiveDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    emissiveDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    emissiveDescriptorInfo.data.pCombinedImageSampler = &emissiveDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &emissiveDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[4]
    );

    // create occlusion texture descriptor
    VkDescriptorImageInfo occlusionDescriptor{};

    occlusionDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    occlusionDescriptor.imageView = resources.occlusionImage.imageView;
    occlusionDescriptor.sampler = resources.occlusionSampler;

    VkDescriptorGetInfoEXT occlusionDescriptorInfo{};
    occlusionDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    occlusionDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    occlusionDescriptorInfo.data.pCombinedImageSampler = &occlusionDescriptor;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &occlusionDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset[5]
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += mMaterialLayoutSize;

    return retOffset;
}

void DescriptorManager::freeResources() {
    VulkanEngine::get().destroyBuffer(mDescriptorBuffer);
}


