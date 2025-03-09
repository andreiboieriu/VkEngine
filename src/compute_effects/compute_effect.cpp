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

ComputeEffect::ComputeEffect(std::string name, VulkanEngine& vkEngine) : mName(name), mVkEngine(vkEngine) {
    load(name);
}

ComputeEffect::ComputeEffect(std::string name, std::span<PushConstantData> defaultPushConstants, VulkanEngine& vkEngine) : ComputeEffect(name, vkEngine) {
    setDefaultPushConstants(defaultPushConstants);
}

ComputeEffect::~ComputeEffect() {
    freeResources();
}

void ComputeEffect::setDefaultPushConstants(std::span<PushConstantData> defaultPushConstants) {
    for (auto& pushData : defaultPushConstants) {
        writePushConstant(pushData.name, pushData.value, pushData.size);
    }
}

void ComputeEffect::load(std::string name) {
    int currPass = 0;

    while (true) {
        // check if pass exists
        std::string path = "shaders/" + name + "_pass" + std::to_string(currPass++) + ".comp.spv";

        if (!std::filesystem::exists(path)) {
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
void ComputeEffect::addSubpass(std::string path) {
    Subpass newSubpass{};

    fmt::println("now loading subpass: {}", path);

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

    // get push constant data
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get push constant block count");

    std::vector<SpvReflectBlockVariable*> pushConstants(count);
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, pushConstants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get push constant blocks");

    newSubpass.pushConstantsSize = pushConstants[0]->size;

    for (uint32_t i = 0; i < pushConstants[0]->member_count; i++) {
        if (mPushInfoMap.contains(pushConstants[0]->members[i].name)) {
            mPushInfoMap[pushConstants[0]->members[i].name].locations.push_back({static_cast<uint32_t>(mSubpasses.size()) ,pushConstants[0]->members[i].offset});
        } else {
            PushConstantInfo pushInfo{};

            pushInfo.locations.push_back({static_cast<uint32_t>(mSubpasses.size()) ,pushConstants[0]->members[i].offset});
            pushInfo.size = pushConstants[0]->members[i].size;
            pushInfo.type = pushConstants[0]->members[i].type_description->type_flags;

            mPushInfoMap[pushConstants[0]->members[i].name] = pushInfo;
        }
    }

    // create push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = newSubpass.pushConstantsSize;
    pushConstantRange.stageFlags = module.shader_stage;

    // create pipeline layout
    newSubpass.pipelineLayout = vkutil::createPipelineLayout(std::vector<VkDescriptorSetLayout>() = {newSubpass.descriptorSetLayout},
                                         std::vector<VkPushConstantRange>() = {pushConstantRange}, mVkEngine.getDevice());

    // create pipeline
    VkShaderModule shaderModule;

    if (!vkutil::loadShaderModule(path.c_str(), mVkEngine.getDevice(), &shaderModule)) {
        fmt::println("Error when building shader: {}", path);
    }

    VkComputePipelineCreateInfo createInfo = vkinit::computePipelineCreateInfo(shaderModule, newSubpass.pipelineLayout);

    VK_CHECK(vkCreateComputePipelines(mVkEngine.getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &newSubpass.pipeline));

    // free resources
    vkDestroyShaderModule(mVkEngine.getDevice(), shaderModule, nullptr);
    spvReflectDestroyShaderModule(&module);

    mSubpasses.push_back(newSubpass);
}

void ComputeEffect::execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize) {
    if (!mEnabled) {
        return;
    }

    writePushConstant("imageExtent", extent);
    writePushConstant("bufferImageExtent", glm::vec2(2560 * 2, 1440 * 2));

    for (unsigned int i = 0; i < mSubpasses.size(); i++) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipeline);

        DescriptorWriter writer;

        for (uint32_t j = 0; j < mSubpasses[i].bindings.size(); j++) {
            BindingData& binding = mSubpasses[i].bindings[j];

            if (binding.name == "image") {
                writer.writeImage(
                    j,
                    image.imageView,
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? mVkEngine.getDefaultLinearSampler() : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else if (binding.name.find("bufferImageMip") != std::string::npos) {
                uint32_t mipLevel = std::stoi(binding.name.substr(std::string("bufferImageMip").size()));

                writer.writeImage(
                    j,
                    mBufferImageMipViews[mipLevel],
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? mVkEngine.getDefaultLinearSampler() : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else {
                fmt::println("Error when executing compute effect {}, undefined binding {}", mName, binding.name);
            }
        }

        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

        vkCmdPushConstants(commandBuffer, mSubpasses[i].pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, mSubpasses[i].pushConstantsSize, &mSubpasses[i].pushConstants);

        vkCmdDispatch(commandBuffer, std::ceil(extent.width / 16.0), std::ceil(extent.height / 16.0), 1);

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

        for (auto& [name, pushInfo] : mPushInfoMap) {
            if (name == "imageExtent" || name == "bufferImageExtent") {
                continue;
            }

            bool isVector = pushInfo.type & SPV_REFLECT_TYPE_FLAG_VECTOR;
            uint32_t count = isVector ? pushInfo.size / 4 : 1;

            bool changeValues = false;

            if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_INT) {
                if (count == 1) {
                    changeValues = ImGui::InputInt(name.c_str(), (int*)(getAddr(pushInfo.locations[0])));
                } else if (count == 2) {
                    changeValues = ImGui::InputInt2(name.c_str(), (int*)(getAddr(pushInfo.locations[0])));
                } else if (count == 3) {
                    changeValues = ImGui::InputInt3(name.c_str(), (int*)(getAddr(pushInfo.locations[0])));
                } else if (count == 4) {
                    changeValues = ImGui::InputInt4(name.c_str(), (int*)(getAddr(pushInfo.locations[0])));
                }
            } else if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_FLOAT) {
                if (count == 1) {
                    changeValues = ImGui::InputFloat(name.c_str(), (float*)(getAddr(pushInfo.locations[0])));
                } else if (count == 2) {
                    changeValues = ImGui::InputFloat2(name.c_str(), (float*)(getAddr(pushInfo.locations[0])));
                } else if (count == 3) {
                    changeValues = ImGui::InputFloat3(name.c_str(), (float*)(getAddr(pushInfo.locations[0])));
                } else if (count == 4) {
                    changeValues = ImGui::InputFloat4(name.c_str(), (float*)(getAddr(pushInfo.locations[0])));
                }
            } else if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_BOOL) {
                changeValues = ImGui::Checkbox(name.c_str(), (bool *)(getAddr(pushInfo.locations[0])));
            }

            if (changeValues) {
                for (uint32_t i = 1; i < pushInfo.locations.size(); i++) {
                    memcpy(getAddr(pushInfo.locations[i]), getAddr(pushInfo.locations[0]), pushInfo.size);
                }
            }
        }

        ImGui::TreePop();
    }
}

void ComputeEffect::writePushConstant(const std::string& name, void* addr, size_t size) {
    if (!mPushInfoMap.contains(name)) {
        return;
    }

    fmt::println("writing push constant: {}", name);

    PushConstantInfo& pushInfo = mPushInfoMap[name];

    if (size != pushInfo.size) {
        fmt::println("write push constant failed: size mismatch");
        return;
    }

    for (auto& location : pushInfo.locations) {
        memcpy(getAddr(location), addr, size);
    }
}
