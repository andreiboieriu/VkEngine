#include "pipeline_resource_manager.h"
#include <fmt/core.h>
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "volk.h"
#include "vk_engine.h"

PipelineResourceManager::PipelineResourceManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    init(mVkEngine.getDevice(), mVkEngine.getDrawImageFormat(), mVkEngine.getDepthImageFormat());
}

PipelineResourceManager::~PipelineResourceManager() {
    freeResources();
}

void PipelineResourceManager::createDescriptorSetLayouts(VkDevice device) {
    {
        DescriptorLayoutBuilder layoutBuilder{};
        DescriptorSetLayoutInfo info{};

        info.layout = layoutBuilder
                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        vkGetDescriptorSetLayoutSizeEXT(device, info.layout, &info.size);
        info.alignedSize = alignedSize(info.size, mDescriptorBufferProperties.descriptorBufferOffsetAlignment);
        info.bindingOffsets.resize(6);

        for (int i = 0; i < 6; i++) {
            vkGetDescriptorSetLayoutBindingOffsetEXT(device, info.layout, i, &info.bindingOffsets[i]);
        }

        mDescriptorSetLayouts[DescriptorSetLayoutType::PBR] = info;
    }

    {
        DescriptorLayoutBuilder layoutBuilder{};
        DescriptorSetLayoutInfo info{};

        info.layout = layoutBuilder
                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                        .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

        vkGetDescriptorSetLayoutSizeEXT(device, info.layout, &info.size);
        info.alignedSize = alignedSize(info.size, mDescriptorBufferProperties.descriptorBufferOffsetAlignment);
        info.bindingOffsets.resize(4);

        for (int i = 0; i < 4; i++) {
            vkGetDescriptorSetLayoutBindingOffsetEXT(device, info.layout, i, &info.bindingOffsets[i]);
        }

        mDescriptorSetLayouts[DescriptorSetLayoutType::PBR_SCENE_DATA] = info;
    }

    {
        DescriptorLayoutBuilder layoutBuilder{};
        DescriptorSetLayoutInfo info{};

        info.layout = layoutBuilder
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

        mDescriptorSetLayouts[DescriptorSetLayoutType::SPRITE] = info;
    }

    {
        DescriptorLayoutBuilder layoutBuilder{};
        DescriptorSetLayoutInfo info{};

        info.layout = layoutBuilder
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
            .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

        mDescriptorSetLayouts[DescriptorSetLayoutType::SKYBOX] = info;
    }
}

void PipelineResourceManager::buildPBRPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    // load shaders
    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/mesh_pbr.frag.spv", device, &fragShader)) {
        fmt::println("error while building mesh.frag shader");
    }

    VkShaderModule vertShader;

    if (!vkutil::loadShaderModule("shaders/mesh_pbr.vert.spv", device, &vertShader)) {
        fmt::println("error while building mesh.vert shader");
    }

    std::vector<VkPushConstantRange> pushConstants(1);
    pushConstants[0].offset = 0;
    pushConstants[0].size = sizeof(PBRPushConstants);
    pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayout layouts[] = {
        mDescriptorSetLayouts[DescriptorSetLayoutType::PBR_SCENE_DATA].layout,
        mDescriptorSetLayouts[DescriptorSetLayoutType::PBR].layout
    };

    VkPipelineLayout newLayout = vkutil::createPipelineLayout(layouts, pushConstants, device);

    mPipelines[PipelineType::PBR_OPAQUE].layout = newLayout;
    mPipelines[PipelineType::PBR_TRANSPARENT].layout = newLayout;
    mPipelines[PipelineType::PBR_OPAQUE_DOUBLE_SIDED].layout = newLayout;
    mPipelines[PipelineType::PBR_TRANSPARENT_DOUBLE_SIDED].layout = newLayout;

    PipelineBuilder pipelineBuilder;

    mPipelines[PipelineType::PBR_OPAQUE_DOUBLE_SIDED].pipeline = pipelineBuilder.clear().setShaders(vertShader, fragShader)
                                              .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                              .setPolygonMode(VK_POLYGON_MODE_FILL)
                                              .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                              .setMultisampling()
                                              .disableBlending()
                                              .enableDepthTesting(true, VK_COMPARE_OP_GREATER)
                                              .setColorAttachmentFormat(colorFormat)
                                              .setDepthFormat(depthFormat)
                                              .setLayout(mPipelines[PipelineType::PBR_OPAQUE].layout)
                                              .buildPipeline(device,  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mPipelines[PipelineType::PBR_OPAQUE].pipeline = pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                              .buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mPipelines[PipelineType::PBR_TRANSPARENT].pipeline = pipelineBuilder.enableBlendingAdditive()
                                                   .enableDepthTesting(false, VK_COMPARE_OP_GREATER)
                                                   .buildPipeline(device,  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mPipelines[PipelineType::PBR_TRANSPARENT_DOUBLE_SIDED].pipeline = pipelineBuilder.setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                                         .buildPipeline(device, VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void PipelineResourceManager::buildPhongPipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {

}

void PipelineResourceManager::buildSpritePipeline(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    std::vector<VkDescriptorSetLayout> setLayouts = {mDescriptorSetLayouts[DescriptorSetLayoutType::SPRITE].layout};
    std::vector<VkPushConstantRange> pushConstants(1);
    pushConstants[0].offset = 0;
    pushConstants[0].size = sizeof(SpritePushConstants);
    pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    // create pipeline layout
    mPipelines[PipelineType::SPRITE].layout = vkutil::createPipelineLayout(
        setLayouts,
        pushConstants,
        device
    );

    // create pipeline
    VkShaderModule vertShader;
    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/sprite.vert.spv", device, &vertShader)) {
        fmt::println("failed to load sprite vertex shader");
    }

    if (!vkutil::loadShaderModule("shaders/sprite.frag.spv", device, &fragShader)) {
        fmt::println("failed to load sprite frag shader");
    }

    PipelineBuilder pipelineBuilder;
    mPipelines[PipelineType::SPRITE].pipeline = pipelineBuilder.clear().setShaders(vertShader, fragShader)
                                              .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                              .setPolygonMode(VK_POLYGON_MODE_FILL)
                                              .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                              .setMultisampling()
                                            //   .enableBlendingAdditive()
                                              .enableBlendingAlphaBlend()
                                              // .disableDepthTesting()
                                              .enableDepthTesting(false, VK_COMPARE_OP_GREATER) // ??
                                              .setColorAttachmentFormat(colorFormat)
                                              .setDepthFormat(depthFormat)
                                              .setLayout(mPipelines[PipelineType::SPRITE].layout)
                                              .buildPipeline(device,  0);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void PipelineResourceManager::createDescriptorBuffers(VkDevice device) {
    // get descriptor buffer properties
    mDescriptorBufferProperties = mVkEngine.getDescriptorBufferProperties();

    static constexpr size_t bufferSize = 256 * 1e5;

    // create descriptor buffer
    mDescriptorBuffer = mVkEngine.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
}

void PipelineResourceManager::buildSkyboxPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    std::vector<VkDescriptorSetLayout> setLayouts = {mDescriptorSetLayouts[DescriptorSetLayoutType::SKYBOX].layout};
    std::vector<VkPushConstantRange> pushConstants(1);
    pushConstants[0].offset = 0;
    pushConstants[0].size = sizeof(SkyboxPushConstants);
    pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // create pipeline layout
    mPipelines[PipelineType::SKYBOX].layout = vkutil::createPipelineLayout(
        setLayouts,
        pushConstants,
        device
    );

    pushConstants[0].size = sizeof(SkyboxLoadPushConstants);
    pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    mPipelines[PipelineType::EQUI_TO_CUBE].layout = vkutil::createPipelineLayout(
        setLayouts,
        pushConstants,
        device
    );

    VkShaderModule vertShader;

    if (!vkutil::loadShaderModule("shaders/skybox.vert.spv", device, &vertShader)) {
        fmt::println("Error when building shader: shaders/skybox.vert.spv");
    }

    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/skybox.frag.spv", device, &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    PipelineBuilder builder;

    mPipelines[PipelineType::SKYBOX].pipeline = builder.setShaders(vertShader, fragShader)
                        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                        .setPolygonMode(VK_POLYGON_MODE_FILL)
                        .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                        .setMultisampling()
                        .disableBlending()
                        .enableDepthTesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                        .setColorAttachmentFormat(mVkEngine.getDrawImageFormat())
                        .setDepthFormat(mVkEngine.getDepthImageFormat())
                        .setLayout(mPipelines[PipelineType::SKYBOX].layout)
                        .buildPipeline(device, 0);

    vkDestroyShaderModule(device, fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_equi_to_cube.frag.spv", device, &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    builder.setLayout(mPipelines[PipelineType::EQUI_TO_CUBE].layout);
    mPipelines[PipelineType::EQUI_TO_CUBE].pipeline = builder.buildPipeline(device, 0);

    vkDestroyShaderModule(device, fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_env_to_irradiance.frag.spv", device, &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    mPipelines[PipelineType::ENV_TO_IRR].pipeline = builder.buildPipeline(device, 0);
    mPipelines[PipelineType::ENV_TO_IRR].layout = mPipelines[PipelineType::EQUI_TO_CUBE].layout;

    vkDestroyShaderModule(device, fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_prefilter_env.frag.spv", device, &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    mPipelines[PipelineType::PREFILTER_ENV].pipeline = builder.buildPipeline(device, 0);
    mPipelines[PipelineType::PREFILTER_ENV].layout = mPipelines[PipelineType::EQUI_TO_CUBE].layout;

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void PipelineResourceManager::bindDescriptorBuffers(VkCommandBuffer commandBuffer) {
    VkDescriptorBufferBindingInfoEXT bindingInfo;

    bindingInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
    bindingInfo.address = mDescriptorBuffer.deviceAddress;
    bindingInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT | VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT;
    bindingInfo.pNext = nullptr;

    vkCmdBindDescriptorBuffersEXT(commandBuffer, 1, &bindingInfo);
}

void PipelineResourceManager::init(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    createDescriptorBuffers(device);
    createDescriptorSetLayouts(device);

    buildPBRPipelines(device, colorFormat, depthFormat);
    buildSpritePipeline(device, colorFormat, depthFormat);
    buildSkyboxPipelines(device, colorFormat, depthFormat);
}

void PipelineResourceManager::freeResources() {
    VkDevice device = mVkEngine.getDevice();

    for (auto& [k, v] : mDescriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(device, v.layout, nullptr);
    }

    // some of these might be shared, so manual deletion is necessary
    vkDestroyPipelineLayout(device, mPipelines[PipelineType::PBR_OPAQUE].layout, nullptr);
    vkDestroyPipelineLayout(device, mPipelines[PipelineType::SPRITE].layout, nullptr);
    vkDestroyPipelineLayout(device, mPipelines[PipelineType::SKYBOX].layout, nullptr);
    vkDestroyPipelineLayout(device, mPipelines[PipelineType::EQUI_TO_CUBE].layout, nullptr);

    for (auto& [k, v] : mPipelines) {
        vkDestroyPipeline(device, v.pipeline, nullptr);
    }

    mVkEngine.destroyBuffer(mDescriptorBuffer);
}

MaterialInstance PipelineResourceManager::writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources) {
    MaterialInstance materialInstance;
    materialInstance.passType = pass;

    if (pass == MaterialPass::Transparent) {
        materialInstance.pipeline = &mPipelines[PipelineType::PBR_TRANSPARENT];
    } else if (pass == MaterialPass::Opaque) {
        materialInstance.pipeline = &mPipelines[PipelineType::PBR_OPAQUE];
    } else if (pass == MaterialPass::TransparentDoubleSided) {
        materialInstance.pipeline = &mPipelines[PipelineType::PBR_TRANSPARENT_DOUBLE_SIDED];
    } else if (pass == MaterialPass::OpaqueDoubleSided) {
        materialInstance.pipeline = &mPipelines[PipelineType::PBR_OPAQUE_DOUBLE_SIDED];
    }

    materialInstance.descriptorOffset = createMaterialDescriptor(resources);

    return materialInstance;
}

VkDeviceSize PipelineResourceManager::createSceneDescriptor(
    VkDeviceAddress bufferAddress,
    VkDeviceSize size,
    VkImageView irradianceMapView,
    VkImageView prefilteredEnvMapView,
    VkImageView brdflutView,
    VkSampler sampler
) {
    auto descriptorSetInfo = mDescriptorSetLayouts[DescriptorSetLayoutType::PBR_SCENE_DATA];

    // create scene data buffer descriptor
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
        mVkEngine.getDevice(),
        &descriptorInfo,
        mDescriptorBufferProperties.uniformBufferDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[0]
    );

    // create irradiance map descriptor
    VkDescriptorImageInfo imageDescriptor{};

    imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageDescriptor.imageView = irradianceMapView;
    imageDescriptor.sampler = sampler;

    VkDescriptorGetInfoEXT imageDescriptorInfo{};
    imageDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    imageDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageDescriptorInfo.data.pCombinedImageSampler = &imageDescriptor;

    vkGetDescriptorEXT(
        mVkEngine.getDevice(),
        &imageDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[1]
    );

    // create prefiltered env map descriptor
    VkDescriptorImageInfo prefilteredDescriptor{};

    prefilteredDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    prefilteredDescriptor.imageView = prefilteredEnvMapView;
    prefilteredDescriptor.sampler = sampler;

    VkDescriptorGetInfoEXT prefilteredDescriptorInfo{};
    prefilteredDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    prefilteredDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    prefilteredDescriptorInfo.data.pCombinedImageSampler = &prefilteredDescriptor;

    vkGetDescriptorEXT(
        mVkEngine.getDevice(),
        &prefilteredDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[2]
    );

    // create brdf lut descriptor
    VkDescriptorImageInfo brdflutDescriptor{};

    brdflutDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdflutDescriptor.imageView = brdflutView;
    brdflutDescriptor.sampler = sampler;

    VkDescriptorGetInfoEXT brdflutDescriptorInfo{};
    brdflutDescriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
    brdflutDescriptorInfo.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    brdflutDescriptorInfo.data.pCombinedImageSampler = &brdflutDescriptor;

    vkGetDescriptorEXT(
        mVkEngine.getDevice(),
        &brdflutDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[3]
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += descriptorSetInfo.alignedSize;

    return retOffset;
}

VkDeviceSize PipelineResourceManager::createMaterialDescriptor(const MaterialResources& resources) {
    auto descriptorSetInfo = mDescriptorSetLayouts[DescriptorSetLayoutType::PBR];

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
        mVkEngine.getDevice(),
        &descriptorInfo,
        mDescriptorBufferProperties.uniformBufferDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[0]
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
        mVkEngine.getDevice(),
        &imageDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[1]
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
        mVkEngine.getDevice(),
        &metalDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[2]
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
        mVkEngine.getDevice(),
        &normalDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[3]
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
        mVkEngine.getDevice(),
        &emissiveDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[4]
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
        mVkEngine.getDevice(),
        &occlusionDescriptorInfo,
        mDescriptorBufferProperties.combinedImageSamplerDescriptorSize,
        (char*)mDescriptorBuffer.allocInfo.pMappedData + mCurrentOffset + descriptorSetInfo.bindingOffsets[5]
    );

    VkDeviceSize retOffset = mCurrentOffset;
    mCurrentOffset += descriptorSetInfo.alignedSize;

    return retOffset;
}
