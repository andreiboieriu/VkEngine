#include "vk_materials.h"
#include "fmt/core.h"
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include "vk_types.h"
#include "volk.h"
#include "vk_engine.h"

MaterialManager::MaterialManager() {
    buildPipelines(VulkanEngine::get().getDevice(), VulkanEngine::get().getDrawImageFormat(), VulkanEngine::get().getDepthImageFormat());
}

MaterialManager::~MaterialManager() {
    freeResources();
}

void MaterialManager::buildPipelines(VkDevice device, VkFormat colorFormat, VkFormat depthFormat) {
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
    pushConstants[0].size = sizeof(GPUDrawPushConstants);
    pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    DescriptorLayoutBuilder layoutBuilder{};
    mMaterialDescriptorLayout = layoutBuilder
                                   .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mSceneDescriptorLayout = layoutBuilder.clear()
                                       .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                       .build(device, nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    VkDescriptorSetLayout layouts[] = {
        mSceneDescriptorLayout,
        mMaterialDescriptorLayout
    };

    VkPipelineLayout newLayout = vkutil::createPipelineLayout(layouts, pushConstants);

    mOpaquePipeline.layout = newLayout;
    mTransparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;

    mOpaquePipeline.pipeline = pipelineBuilder.clear().setShaders(vertShader, fragShader)
                                              .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                              .setPolygonMode(VK_POLYGON_MODE_FILL)
                                              .setCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                              .setMultisampling()
                                              .disableBlending()
                                              .enableDepthTesting(true, VK_COMPARE_OP_GREATER)
                                              .setColorAttachmentFormat(colorFormat)
                                              .setDepthFormat(depthFormat)
                                              .setLayout(mOpaquePipeline.layout)
                                              .buildPipeline(device,  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    mTransparentPipeline.pipeline = pipelineBuilder.enableBlendingAdditive()
                                                   .enableDepthTesting(false, VK_COMPARE_OP_GREATER)
                                                   .buildPipeline(device,  VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void MaterialManager::freeResources() {
    VkDevice device = VulkanEngine::get().getDevice();

    vkDestroyDescriptorSetLayout(device, mSceneDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, mMaterialDescriptorLayout, nullptr);

    vkDestroyPipelineLayout(device, mOpaquePipeline.layout, nullptr);

    vkDestroyPipeline(device, mOpaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, mTransparentPipeline.pipeline, nullptr);
}

MaterialInstance MaterialManager::writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources) {
    MaterialInstance materialInstance;
    materialInstance.passType = pass;

    if (pass == MaterialPass::Transparent) {
        materialInstance.pipeline = &mTransparentPipeline;
    } else {
        materialInstance.pipeline = &mOpaquePipeline;
    }

    materialInstance.descriptorOffset = VulkanEngine::get().getDescriptorManager().createMaterialDescriptor(resources);


    return materialInstance;
}
