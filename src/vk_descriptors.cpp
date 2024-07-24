#include "vk_descriptors.h"
#include "vk_types.h"
#include "volk.h"
#include "vk_engine.h"

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type) {
    VkDescriptorSetLayoutBinding newBind{};
    newBind.binding = binding;
    newBind.descriptorCount = 1;
    newBind.descriptorType = type;

    mBindings.push_back(newBind);

    return *this;
}

DescriptorLayoutBuilder& DescriptorLayoutBuilder::clear() {
    mBindings.clear();

    return *this;
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

void DynamicDescriptorAllocator::init(VkDevice device, uint32_t initialSets, std::span<DynamicDescriptorAllocator::PoolSizeRatio> poolRatios) {
    mRatios.clear();

    for (auto ratio : poolRatios) {
        mRatios.push_back(ratio);
    }

    VkDescriptorPool newPool = createPool(device, initialSets, poolRatios);
    mSetsPerPool = initialSets * 1.5;

    mAvailablePools.push_back(newPool);
}

void DynamicDescriptorAllocator::clearPools(VkDevice device) {
    for (auto pool : mAvailablePools) {
        vkResetDescriptorPool(device, pool, 0);
    }

    for (auto pool : mFullPools) {
        vkResetDescriptorPool(device, pool, 0);
        mAvailablePools.push_back(pool);
    }

    mFullPools.clear();
}

void DynamicDescriptorAllocator::destroyPools(VkDevice device) {
    for (auto pool : mAvailablePools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    for (auto pool : mFullPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    mAvailablePools.clear();
    mFullPools.clear();
}

VkDescriptorSet DynamicDescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext) {
    VkDescriptorPool pool = getPool(device);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.pNext = pNext;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
        mFullPools.push_back(pool);

        pool = getPool(device);
        allocInfo.descriptorPool = pool;

        VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
    }

    mAvailablePools.push_back(pool);

    return descriptorSet;
}

VkDescriptorPool DynamicDescriptorAllocator::getPool(VkDevice device) {
    VkDescriptorPool newPool;

    if (!mAvailablePools.empty()) {
        newPool = mAvailablePools.back();
        mAvailablePools.pop_back();

        return newPool;
    }

    newPool = createPool(device, mSetsPerPool, mRatios);

    mSetsPerPool = mSetsPerPool * 1.5;

    if (mSetsPerPool > mMaxSetsPerPool) {
        mSetsPerPool = mMaxSetsPerPool;
    }

    return newPool;
}

VkDescriptorPool DynamicDescriptorAllocator::createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) {
    std::vector<VkDescriptorPoolSize> poolSizes;

    for (PoolSizeRatio ratio : poolRatios) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = ratio.type,
            .descriptorCount = uint32_t(ratio.ratio * setCount)
        });
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = 0;
    poolInfo.maxSets = setCount;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool));

    return newPool;
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
    VulkanEngine::get().destroyBuffer(mUniformDescriptorBuffer);
    VulkanEngine::get().destroyBuffer(mImageDescriptorBuffer);
}

void DescriptorManager::init() {

}

void DescriptorManager::createDescriptorBuffers() {
    VkDevice& device = VulkanEngine::get().getDevice();

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
    mUniformDescriptorBuffer = VulkanEngine::get().createBuffer(
        mGlobalLayoutSize,
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );

    mImageDescriptorBuffer = VulkanEngine::get().createBuffer(
        mMaterialLayoutSize * 10000,
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );
}

void DescriptorManager::bindDescriptorBuffers(VkCommandBuffer commandBuffer) {
    VkDescriptorBufferBindingInfoEXT bindingInfo[2];
    bindingInfo[0].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindingInfo[0].address = mUniformDescriptorBuffer.deviceAddress;
    bindingInfo[0].usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;\
    bindingInfo[0].pNext = nullptr;

    bindingInfo[1].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindingInfo[1].address = mImageDescriptorBuffer.deviceAddress;
    bindingInfo[1].usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    bindingInfo[1].pNext = nullptr;

    vkCmdBindDescriptorBuffersEXT(commandBuffer, 2, bindingInfo);
}
 
VkDeviceSize DescriptorManager::createGlobalDescriptor(VkDeviceAddress bufferAddress, VkDeviceSize size) {
    VkDescriptorAddressInfoEXT addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    addressInfo.address = bufferAddress;
    addressInfo.range = size;
    addressInfo.format = VK_FORMAT_UNDEFINED;

    VkDescriptorGetInfoEXT descriptorInfo{};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorInfo.data.pUniformBuffer = &addressInfo;

    VkDeviceSize retOffset = mUniformCurrentOffset;

    vkGetDescriptorEXT(
        VulkanEngine::get().getDevice(),
        &descriptorInfo,
        mDescriptorBufferProperties.uniformBufferDescriptorSize,
        (char*)mUniformDescriptorBuffer.allocInfo.pMappedData + mUniformCurrentOffset + mGlobalLayoutOffset
    );

    mUniformCurrentOffset += mGlobalLayoutSize;

    return retOffset;
}

VkDeviceSize DescriptorManager::createMaterialDescriptor(const MaterialResources& resources) {
    // // create material constants descriptor
    // VkDescriptorAddressInfoEXT addressInfo{};
    // addressInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
    // addressInfo.address = resources.dataBufferAddress + resources.dataBufferOffset;
    // addressInfo.range = sizeof(MaterialConstants);
    // addressInfo.format = VK_FORMAT_UNDEFINED;

    // VkDescriptorGetInfoEXT descriptorInfo{};
    // descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    // descriptorInfo.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // descriptorInfo.data.pUniformBuffer = &addressInfo;


    VkDeviceSize retOffset = mImageCurrentOffset;

    // vkGetDescriptorEXT(
    //     VulkanEngine::get().getDevice(),
    //     &descriptorInfo,
    //     mDescriptorBufferProperties.uniformBufferDescriptorSize,
    //     (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + mMaterialLayoutOffset
    // );

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
        (char*)mImageDescriptorBuffer.allocInfo.pMappedData + mImageCurrentOffset + mMaterialLayoutOffset
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
        (char*)mImageDescriptorBuffer.allocInfo.pMappedData + mImageCurrentOffset + mMaterialLayoutOffset + mDescriptorBufferProperties.combinedImageSamplerDescriptorSize
    );

    mImageCurrentOffset += mMaterialLayoutSize;

    return retOffset;
}


