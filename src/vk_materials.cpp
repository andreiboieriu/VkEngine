#include "vk_materials.h"
#include "fmt/core.h"
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "volk.h"
#include <vulkan/vulkan_core.h>

void GLTFMetallicRoughness::buildPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
    // load shaders
    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/mesh.frag.spv", device, &fragShader)) {
        fmt::println("error while building mesh.frag shader");
    }

    VkShaderModule vertShader;

    if (!vkutil::loadShaderModule("shaders/mesh.vert.spv", device, &vertShader)) {
        fmt::println("error while building mesh.vert shader");
    }

    VkPushConstantRange pushConstants;
    pushConstants.offset = 0;
    pushConstants.size = sizeof(GPUDrawPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder{};
    mMaterialLayout = layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                   .build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    mGpuSceneDataLayout = layoutBuilder.clear()
                                       .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                       .build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    VkDescriptorSetLayout layouts[] = {
        mGpuSceneDataLayout,
        mMaterialLayout
    };

    VkPipelineLayoutCreateInfo meshLayoutInfo = vkinit::pipeline_layout_create_info();
    meshLayoutInfo.setLayoutCount = 2;
    meshLayoutInfo.pSetLayouts = layouts;
    meshLayoutInfo.pPushConstantRanges = &pushConstants;
    meshLayoutInfo.pushConstantRangeCount = 1;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(device, &meshLayoutInfo, nullptr, &newLayout));

    mOpaquePipeline.layout = newLayout;
    mTransparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;

    mOpaquePipeline.pipeline = pipelineBuilder.setShaders(vertShader, fragShader)
                                              .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                              .setPolygonMode(VK_POLYGON_MODE_FILL)
                                              .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                                              .setMultisampling()
                                              .disableBlending()
                                              .enableDepthTesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                                              .setColorAttachmentFormat(colorFormat)
                                              .setDepthFormat(depthFormat)
                                              .setLayout(mOpaquePipeline.layout)
                                              .buildPipeline(device);

    mTransparentPipeline.pipeline =  pipelineBuilder.enableBlendingAdditive()
                                                    .enableDepthTesting(false, VK_COMPARE_OP_GREATER_OR_EQUAL)
                                                    .buildPipeline(device);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void GLTFMetallicRoughness::clearResources(VkDevice device) {
    
}

MaterialInstance GLTFMetallicRoughness::writeMaterial(VkDevice device, MaterialPass pass, const GLTFMetallicRoughness::MaterialResources& resources) {
    MaterialInstance materialInstance;
    materialInstance.passType = pass;

    if (pass == MaterialPass::Transparent) {
        materialInstance.pipeline = &mTransparentPipeline;
    } else {
        materialInstance.pipeline = &mOpaquePipeline;
    }

    materialInstance.descriptorWrites = mDescriptorWriter.clear()
                                                         .writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                                         .writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                         .writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                         .getWrites();
    return materialInstance;
}
