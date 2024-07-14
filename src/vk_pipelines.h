#pragma once 
#include "vk_types.h"

namespace vkutil {

bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule* outShaderModule);

};

class PipelineBuilder {
public:
    PipelineBuilder() {
        clear();
    }

    void clear();

    PipelineBuilder& setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    PipelineBuilder& setInputTopology(VkPrimitiveTopology topology);
    PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);
    PipelineBuilder& setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
    PipelineBuilder& setMultisampling();
    PipelineBuilder& disableBlending();
    PipelineBuilder& setColorAttachmentFormat(VkFormat format);
    PipelineBuilder& setDepthFormat(VkFormat format);
    PipelineBuilder& disableDepthTesting();

    PipelineBuilder& setLayout(VkPipelineLayout layout);
    VkPipeline buildPipeline(VkDevice device);

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