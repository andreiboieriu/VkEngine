#pragma once 
#include "vk_types.h"
#include <span>

namespace vkutil {

bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);
VkPipelineLayout createPipelineLayout(std::span<VkDescriptorSetLayout> descriptorSetLayouts, std::span<VkPushConstantRange> pushConstantRanges);

}; // namespace vkutil

class PipelineBuilder {
public:
    PipelineBuilder() {
        clear();
    }

    PipelineBuilder& clear();

    PipelineBuilder& setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& setInputTopology(VkPrimitiveTopology topology);
    PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& setMultisampling();
    PipelineBuilder& disableBlending();
    PipelineBuilder& enableBlendingAdditive();
    PipelineBuilder& enableBlendingAlphaBlend();
    PipelineBuilder& setColorAttachmentFormat(VkFormat format);
    PipelineBuilder& setDepthFormat(VkFormat format);
    PipelineBuilder& disableDepthTesting();
    PipelineBuilder& enableDepthTesting(bool depthWriteEnable, VkCompareOp op);

    PipelineBuilder& setLayout(VkPipelineLayout layout);
    VkPipeline buildPipeline(VkDevice device, VkPipelineCreateFlags flags);

private:
    std::vector<VkPipelineShaderStageCreateInfo> mShaderStages;

    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
    VkPipelineRasterizationStateCreateInfo mRasterizer;
    VkPipelineColorBlendAttachmentState mColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo mMultisampling;
    VkPipelineLayout mPipelineLayout;
    VkPipelineDepthStencilStateCreateInfo mDepthStencil;
    VkPipelineRenderingCreateInfo mRenderInfo;
    VkFormat mColorAttachmentformat;


};