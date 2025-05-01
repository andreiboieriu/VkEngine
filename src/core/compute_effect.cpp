#include "compute_effect.h"
#include <filesystem>
#include <fstream>

#include "spirv_reflect.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include <imgui.h>

struct SpirvFile {
    std::vector<uint32_t> code;
    size_t byteSize;
};

struct DescriptorSetLayoutData {
    uint32_t set_number;
    VkDescriptorSetLayoutCreateInfo create_info;
};

SpirvFile loadSpirvFile(std::string path) {
    SpirvFile newSpirvFile{};

    // open shader file
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    assert(file.is_open() && ("failed to load spirv file: " + path).c_str());

    // get file size
    newSpirvFile.byteSize = (size_t)file.tellg();

    // create uint32_t buffer
    std::vector<uint32_t> buffer(newSpirvFile.byteSize / sizeof(uint32_t));

    // set file cursor at the beginning
    file.seekg(0);

    // load file into buffer
    file.read((char*)buffer.data(), newSpirvFile.byteSize);

    // close file
    file.close();

    newSpirvFile.code = buffer;

    return newSpirvFile;
}

ComputeEffect::ComputeEffect(const std::string& name, VulkanEngine& vkEngine, bool isEffect) : mName(name), mVkEngine(vkEngine) {
    load(name, isEffect);
}

ComputeEffect::ComputeEffect(const std::string& name, const std::vector<ComputeEffect::EditableSubpassInfo>& info, VulkanEngine& vkEngine, bool isEffect) : ComputeEffect(name, vkEngine, isEffect) {
    setSubpassInfo(info);
}

ComputeEffect::~ComputeEffect() {
    freeResources();
}

void ComputeEffect::setSubpassInfo(const std::vector<ComputeEffect::EditableSubpassInfo>& info) {
    for (int i = 0; i < mSubpasses.size(); i++) {
        mSubpasses[i].editableInfo = info[i];
    }
}

void ComputeEffect::load(const std::string& name, bool isEffect) {
    int currPass = 0;

    while (true) {
        // check if pass exists
        std::string path = isEffect ? "assets/compute_effects/" : "shaders/compute/";
        path += name + "_pass" + std::to_string(currPass++) + ".comp.spv";

        if (!std::filesystem::exists(path)) {
            fmt::println("Compute effect {} not found", path);
            break;
        }

        // create new subpass
        addSubpass(path);
    };

    // create buffer image
    if (mUseBufferImage) {
        mBufferImage = mVkEngine.createImage(VkExtent3D{2560 * 2, 1440 * 2, 1}, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, true);

        mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
            vkutil::transitionImage(commandBuffer, mBufferImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        });

        mBufferImageMipViews.resize(mBufferImage.mipLevels);

        for (uint32_t i = 0; i < mBufferImage.mipLevels; i++) {
            VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(VK_FORMAT_R16G16B16A16_SFLOAT, mBufferImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseMipLevel = i;

            VK_CHECK(vkCreateImageView(mVkEngine.getDevice(), &viewInfo, nullptr, &mBufferImageMipViews[i]));
        }
    }
}

// TODO: see about generalizing descriptor writes
void ComputeEffect::addSubpass(const std::filesystem::path& path) {
    Subpass newSubpass{};

    fmt::println("now loading subpass: {}", path.c_str());

    // load spirv file
    SpirvFile spirvFile = loadSpirvFile(path);
    SpvReflectShaderModule module;

    SpvReflectResult result = spvReflectCreateShaderModule(spirvFile.byteSize, spirvFile.code.data(), &module);
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "failed to create spv shader module");

    // get descriptor set data
    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get descriptor set count");
    assert(count == 1 && "more than 1 descriptor set found");

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&module, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get descriptor sets");

    DescriptorSetLayoutData setLayout{};

    const SpvReflectDescriptorSet& reflSet = *(sets[0]);
    newSubpass.bindings.resize(reflSet.binding_count);

    // create descriptor set layout
    DescriptorLayoutBuilder layoutBuilder{};

    for (uint32_t i = 0; i < reflSet.binding_count; i++) {
        const SpvReflectDescriptorBinding& reflBinding = *(reflSet.bindings[i]);

        if (!mUseBufferImage && std::string(reflBinding.name).find("bufferImage") != std::string::npos) {
            mUseBufferImage = true;
        }

        newSubpass.bindings[reflBinding.binding] = {reflBinding.name, static_cast<VkDescriptorType>(reflBinding.descriptor_type)};

        layoutBuilder.addBinding(reflBinding.binding, static_cast<VkDescriptorType>(reflBinding.descriptor_type), module.shader_stage);
    }

    newSubpass.descriptorSetLayout = layoutBuilder.build(mVkEngine.getDevice(), nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    // get local workgroup size
    auto entryPoint = module.entry_points[0];
    auto localSize = entryPoint.local_size;

    newSubpass.localSize = glm::vec3(localSize.x, localSize.y, localSize.z);

    // create push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = Subpass::pushConstantsSize;
    pushConstantRange.stageFlags = module.shader_stage;

    // create pipeline layout
    newSubpass.pipelineLayout = vkutil::createPipelineLayout(std::vector<VkDescriptorSetLayout>() = {newSubpass.descriptorSetLayout},
                                         std::vector<VkPushConstantRange>() = {pushConstantRange}, mVkEngine.getDevice());

    // create pipeline
    VkShaderModule shaderModule;

    if (!vkutil::loadShaderModule(path.c_str(), mVkEngine.getDevice(), &shaderModule)) {
        fmt::println("Error when building compute shader: {}", path.c_str());
    }

    VkComputePipelineCreateInfo createInfo = vkinit::computePipelineCreateInfo(shaderModule, newSubpass.pipelineLayout);

    VK_CHECK(vkCreateComputePipelines(mVkEngine.getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &newSubpass.pipeline));

    // free resources
    vkDestroyShaderModule(mVkEngine.getDevice(), shaderModule, nullptr);
    spvReflectDestroyShaderModule(&module);

    mSubpasses.push_back(newSubpass);
}

void ComputeEffect::execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize, VkSampler sampler) {
    if (!mEnabled) {
        return;
    }

    for (unsigned int i = 0; i < mSubpasses.size(); i++) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipeline);

        DescriptorWriter writer;

        for (uint32_t j = 0; j < mSubpasses[i].bindings.size(); j++) {
            BindingData& binding = mSubpasses[i].bindings[j];

            if (binding.name == "image") {
                writer.writeImage(
                    j,
                    image.imageView,
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? sampler : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else if (binding.name.find("bufferImageMip") != std::string::npos) {
                uint32_t mipLevel = std::stoi(binding.name.substr(std::string("bufferImageMip").size()));

                writer.writeImage(
                    j,
                    mBufferImageMipViews[mipLevel],
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? sampler : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else {
                fmt::println("Error when executing compute effect {}, undefined binding {}", mName, binding.name);
            }
        }

        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

        // write push constants (image and buffer extent)
        mSubpasses[i].editableInfo.pushConstants[4].x = extent.width;
        mSubpasses[i].editableInfo.pushConstants[4].y = extent.height;
        mSubpasses[i].editableInfo.pushConstants[4].z = 2560 * 2;
        mSubpasses[i].editableInfo.pushConstants[4].w = 1440 * 2;

        vkCmdPushConstants(commandBuffer, mSubpasses[i].pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, mSubpasses[i].pushConstantsSize, mSubpasses[i].editableInfo.pushConstants.data());

        glm::vec2 dispatchSize;

        if (mSubpasses[i].editableInfo.dispatchSize.useScreenSize) {
            dispatchSize = glm::vec2(extent.width, extent.height);
            dispatchSize *= mSubpasses[i].editableInfo.dispatchSize.screenSizeMultiplier;
        } else {
            dispatchSize = mSubpasses[i].editableInfo.dispatchSize.customSize;
        }

        vkCmdDispatch(
            commandBuffer,
            std::ceil(dispatchSize.x / mSubpasses[i].localSize.x),
            std::ceil(dispatchSize.y / mSubpasses[i].localSize.y),
            mSubpasses[i].localSize.z
        );

        if (i < mSubpasses.size() - 1) {
            synchronizeWithCompute(commandBuffer);
        }
    }

    if (synchronize) {
        synchronizeWithCompute(commandBuffer);
    }
}

void ComputeEffect::synchronizeWithCompute(VkCommandBuffer commandBuffer) {
    VkMemoryBarrier2 memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    memoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    memoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;
    depInfo.memoryBarrierCount = 1;
    depInfo.pMemoryBarriers = &memoryBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void ComputeEffect::freeResources() {
    VkDevice device = mVkEngine.getDevice();

    for (auto& subpass : mSubpasses) {
        vkDestroyDescriptorSetLayout(device, subpass.descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, subpass.pipelineLayout, nullptr);
        vkDestroyPipeline(device, subpass.pipeline, nullptr);
    }

    if (mUseBufferImage) {
        mVkEngine.destroyImage(mBufferImage);

        for (uint32_t i = 0; i < mBufferImageMipViews.size(); i++) {
            vkDestroyImageView(device, mBufferImageMipViews[i], nullptr);
        }
    }
}

void ComputeEffect::drawGui() {
    if (ImGui::TreeNode(mName.c_str())) {
        ImGui::Checkbox("enable", &mEnabled);

        for (int i = 0; i < mSubpasses.size(); i++) {
            if (ImGui::TreeNode(("subpass_" + std::to_string(i)).c_str())) {
                for (int j = 0; j < 5; j++) {
                    ImGui::InputFloat4(("values_" + std::to_string(j)).c_str(), (float*)&mSubpasses[i].editableInfo.pushConstants[j]);
                }

                ImGui::Checkbox("use screen size", &mSubpasses[i].editableInfo.dispatchSize.useScreenSize);
                ImGui::InputFloat("screen size multiplier", &mSubpasses[i].editableInfo.dispatchSize.screenSizeMultiplier);
                ImGui::InputFloat2("custom size", (float *)&mSubpasses[i].editableInfo.dispatchSize.customSize);

                ImGui::TreePop();
            }
        }

        ImGui::TreePop();
    }
}
