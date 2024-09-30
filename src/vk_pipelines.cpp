#include "vk_pipelines.h"

#include <fstream>
#include "volk.h"
#include "vk_initializers.h"
#include "vk_engine.h"

bool vkutil::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule) {
    // open shader file
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        return false;
    }

    // get file size
    size_t fileSize = (size_t)file.tellg();

    // create uint32_t buffer
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    // set file cursor at the beginning
    file.seekg(0);

    // load file into buffer
    file.read((char*)buffer.data(), fileSize);

    // close file
    file.close();

    // create new shader module
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t); // size in bytes
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

    *outShaderModule = shaderModule;

    return true;
}

VkPipelineLayout vkutil::createPipelineLayout(std::span<VkDescriptorSetLayout> descriptorSetLayouts, std::span<VkPushConstantRange> pushConstantRanges) {
    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();

    if (!descriptorSetLayouts.empty()) {
        layoutInfo.setLayoutCount = descriptorSetLayouts.size();
        layoutInfo.pSetLayouts = descriptorSetLayouts.data();
    }

    if (!pushConstantRanges.empty()) {
        layoutInfo.pushConstantRangeCount = pushConstantRanges.size();
        layoutInfo.pPushConstantRanges = pushConstantRanges.data();
    }

    VkPipelineLayout layout{};
    VK_CHECK(vkCreatePipelineLayout(VulkanEngine::get().getDevice(), &layoutInfo, nullptr, &layout));

    return layout;
}


PipelineBuilder& PipelineBuilder::clear() {
    mInputAssembly = {};
    mInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

    mRasterizer = {};
    mRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

    mColorBlendAttachment = {};

    mMultisampling = {};
    mMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    mPipelineLayout = {};

    mDepthStencil = {};
    mDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    mRenderInfo = {};
    mRenderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    mShaderStages.clear();

    return *this;
}

PipelineBuilder& PipelineBuilder::setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader) {
    mShaderStages.clear();
    mShaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
    mShaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));

    return *this;
}

PipelineBuilder& PipelineBuilder::setInputTopology(VkPrimitiveTopology topology) {
    mInputAssembly.topology = topology;
    mInputAssembly.primitiveRestartEnable = VK_FALSE;

    return *this;
}

// wireframe vs solid vs point rendering
PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode polygonMode) {
    mRasterizer.polygonMode = polygonMode;
    mRasterizer.lineWidth = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace) {
    mRasterizer.cullMode = cullMode;
    mRasterizer.frontFace = frontFace;

    return *this;
}

PipelineBuilder& PipelineBuilder::setMultisampling() {
    mMultisampling.sampleShadingEnable = VK_FALSE;
    mMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    mMultisampling.minSampleShading = 1.0f;
    mMultisampling.pSampleMask = nullptr;
    mMultisampling.alphaToCoverageEnable = VK_FALSE;
    mMultisampling.alphaToOneEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::disableBlending() {
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_FALSE;

    return *this;
}

PipelineBuilder& PipelineBuilder::setColorAttachmentFormat(VkFormat format) {
    mColorAttachmentformat = format;

    mRenderInfo.colorAttachmentCount = 1;
    mRenderInfo.pColorAttachmentFormats = &mColorAttachmentformat;

    return *this;
}

PipelineBuilder& PipelineBuilder::setDepthFormat(VkFormat format) {
    mRenderInfo.depthAttachmentFormat = format;

    return *this;
}

PipelineBuilder& PipelineBuilder::disableDepthTesting() {
    mDepthStencil.depthTestEnable = VK_FALSE;
    mDepthStencil.depthWriteEnable = VK_FALSE;
    mDepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = {};
    mDepthStencil.back = {};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::enableDepthTesting(bool depthWriteEnable, VkCompareOp op) {
    mDepthStencil.depthTestEnable = VK_TRUE;
    mDepthStencil.depthWriteEnable = depthWriteEnable;
    mDepthStencil.depthCompareOp = op;
    mDepthStencil.depthBoundsTestEnable = VK_FALSE;
    mDepthStencil.stencilTestEnable = VK_FALSE;
    mDepthStencil.front = {};
    mDepthStencil.back = {};
    mDepthStencil.minDepthBounds = 0.f;
    mDepthStencil.maxDepthBounds = 1.f;

    return *this;
}

PipelineBuilder& PipelineBuilder::setLayout(VkPipelineLayout layout) {
    mPipelineLayout = layout;

    return *this;
}

PipelineBuilder& PipelineBuilder::enableBlendingAdditive() {
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_TRUE;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    mColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    return *this;
}

PipelineBuilder& PipelineBuilder::enableBlendingAlphaBlend() {
    mColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    mColorBlendAttachment.blendEnable = VK_TRUE;
    mColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    mColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    mColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    mColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    mColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    mColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    return *this;
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device, VkPipelineCreateFlags flags) {
    // create viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;

    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // setup color blending
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &mColorBlendAttachment;

    // setup vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // setup pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &mRenderInfo;
    pipelineInfo.stageCount = (uint32_t)mShaderStages.size();
    pipelineInfo.pStages = mShaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &mInputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &mRasterizer;
    pipelineInfo.pMultisampleState = &mMultisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDepthStencilState = &mDepthStencil;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.flags = flags;

    // setup dynamic state
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicInfo{};
    dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicInfo.pDynamicStates = dynamicStates;
    dynamicInfo.dynamicStateCount = 2;

    pipelineInfo.pDynamicState = &dynamicInfo;

    VkPipeline newPipeline;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
        fmt::println("failed to create pipeline");
        return VK_NULL_HANDLE;
    } else {
        return newPipeline;
    }
}

