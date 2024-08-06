#include "fxaa.h"

#include "vk_pipelines.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_initializers.h"
#include "imgui.h"

Fxaa::Fxaa() {
    createPipelineLayout();
    addSubpass("shaders/fxaa_pass0.comp.spv");
    addSubpass("shaders/fxaa_pass1.comp.spv");

    mPushConstants.fixedThreshold = 0.0312f;
    mPushConstants.relativeThreshold = 0.063f;
    mPushConstants.pixelBlendStrength = 0.75f;
    mPushConstants.quality = 0;
}

Fxaa::~Fxaa() {
    freeResources();
}


void Fxaa::createPipelineLayout() {
    // create descriptor set layout
    DescriptorLayoutBuilder builder;
    mDescriptorSetLayout = builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                                  .build(VulkanEngine::get().getDevice(), nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    std::vector<VkDescriptorSetLayout> setLayouts = {mDescriptorSetLayout};

    // create push constant ranges
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    std::vector<VkPushConstantRange> pushConstantRanges = {pushConstantRange};

    mPipelineLayout = vkutil::createPipelineLayout(setLayouts, pushConstantRanges);
}

void Fxaa::addSubpass(std::string_view shaderPath) {
    VkShaderModule shaderModule;

    if (!vkutil::loadShaderModule(shaderPath.cbegin(), VulkanEngine::get().getDevice(), &shaderModule)) {
        fmt::println("Error when building shader: {}", shaderPath);
    }

    VkComputePipelineCreateInfo createInfo = vkinit::computePipelineCreateInfo(shaderModule, mPipelineLayout);

    Subpass newSubpass{};

    VK_CHECK(vkCreateComputePipelines(VulkanEngine::get().getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &newSubpass.pipeline));

    mSubpasses.push_back(newSubpass);

    vkDestroyShaderModule(VulkanEngine::get().getDevice(), shaderModule, nullptr);
}

void Fxaa::execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent) {
    if (!mEnabled) {
        return;
    }

    for (unsigned int i = 0; i < mSubpasses.size(); i++) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipeline);

        DescriptorWriter writer;
        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, image.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                                                   .getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mPipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());


        mPushConstants.imageExtent.x = extent.width;
        mPushConstants.imageExtent.y = extent.height;

        vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &mPushConstants);

        vkCmdDispatch(commandBuffer, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 1);

        // synchronize with next subpass
        if (i == mSubpasses.size() - 1) {
            continue;
        }

        VkMemoryBarrier2 memoryBarrier{};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memoryBarrier.pNext = nullptr;
        memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        // memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
        memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        // memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;
        depInfo.memoryBarrierCount = 1;
        depInfo.pMemoryBarriers = &memoryBarrier;

        vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    }
}

void Fxaa::drawGui() {
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("fxaa", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("enable", &mEnabled);
        ImGui::SliderFloat("fixed threshold", &mPushConstants.fixedThreshold, 0.0312f, 0.0833f);
        ImGui::SliderFloat("relative threshold", &mPushConstants.relativeThreshold, 0.063f, 0.333f);
        ImGui::SliderFloat("pixel blend factor", &mPushConstants.pixelBlendStrength, 0.0f, 1.0f);
        ImGui::SliderInt("quality", &mPushConstants.quality, 0, 2);
    }

    ImGui::End();
}

void Fxaa::freeResources() {
    VkDevice device = VulkanEngine::get().getDevice();

    vkDestroyDescriptorSetLayout(device, mDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);

    for (auto& subpass : mSubpasses) {
        vkDestroyPipeline(device, subpass.pipeline, nullptr);
    }
}