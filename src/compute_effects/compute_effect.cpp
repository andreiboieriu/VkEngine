#include "compute_effect.h"
#include <filesystem>
#include <fstream>

#include "spirv_reflect.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_pipelines.h"
#include "vk_initializers.h"
#include "imgui.h"

struct SpirvFile {
    std::vector<uint32_t> code;
    size_t byteSize;
};

struct DescriptorSetLayoutData {
  uint32_t set_number;
  VkDescriptorSetLayoutCreateInfo create_info;
  std::vector<VkDescriptorSetLayoutBinding> bindings;
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

ComputeEffect::ComputeEffect(std::string name) : mName(name) {
    load(name);
}

ComputeEffect::ComputeEffect(std::string name, std::span<PushConstantData> defaultPushConstants) : ComputeEffect(name) {
    setDefaultPushConstants(defaultPushConstants);
}

ComputeEffect::~ComputeEffect() {
    freeResources();
}

void ComputeEffect::setDefaultPushConstants(std::span<PushConstantData> defaultPushConstants) {
    for (auto& subpass : mSubpasses) {
        for (auto& pushInfo : subpass.pushInfos) {
            for (auto& pushData : defaultPushConstants) {
                if (pushData.name == pushInfo.name) {
                    for (uint32_t i = 0; i < pushData.size; i++) {
                        subpass.pushConstants[pushInfo.offset + i] = pushData.value[i];
                    }
                }
            }
        }
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
        mSubpasses.push_back(createSubpass(path));
    };
}

// TODO: see about generalizing descriptor writes
ComputeEffect::Subpass ComputeEffect::createSubpass(std::string path) {
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
    setLayout.bindings.resize(reflSet.binding_count);

    // create descriptor set layout
    DescriptorLayoutBuilder layoutBuilder{};

    for (uint32_t i = 0; i < reflSet.binding_count; i++) {
        const SpvReflectDescriptorBinding& reflBinding = *(reflSet.bindings[i]);

        layoutBuilder.addBinding(reflBinding.binding, static_cast<VkDescriptorType>(reflBinding.descriptor_type), module.shader_stage);
    }

    newSubpass.descriptorSetLayout = layoutBuilder.build(VulkanEngine::get().getDevice(), nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    // get push constant data
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get push constant block count");

    std::vector<SpvReflectBlockVariable*> pushConstants(count);
    result = spvReflectEnumeratePushConstantBlocks(&module, &count, pushConstants.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS && "spv reflect failed to get push constant blocks");

    newSubpass.pushConstantsSize = pushConstants[0]->size;
    newSubpass.pushInfos.resize(pushConstants[0]->member_count);

    for (uint32_t i = 0; i < pushConstants[0]->member_count; i++) {

        newSubpass.pushInfos[i].name = pushConstants[0]->members[i].name;
        newSubpass.pushInfos[i].offset = pushConstants[0]->members[i].offset;
        newSubpass.pushInfos[i].size = pushConstants[0]->members[i].size;
        newSubpass.pushInfos[i].type = pushConstants[0]->members[i].type_description->type_flags;
    }

    // create push constant range
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = newSubpass.pushConstantsSize;
    pushConstantRange.stageFlags = module.shader_stage;

    // create pipeline layout
    newSubpass.pipelineLayout = vkutil::createPipelineLayout(std::vector<VkDescriptorSetLayout>() = {newSubpass.descriptorSetLayout},
                                         std::vector<VkPushConstantRange>() = {pushConstantRange});
    
    // create pipeline
    VkShaderModule shaderModule;

    if (!vkutil::loadShaderModule(path.c_str(), VulkanEngine::get().getDevice(), &shaderModule)) {
        fmt::println("Error when building shader: {}", path);
    }

    VkComputePipelineCreateInfo createInfo = vkinit::computePipelineCreateInfo(shaderModule, newSubpass.pipelineLayout);

    VK_CHECK(vkCreateComputePipelines(VulkanEngine::get().getDevice(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &newSubpass.pipeline));

    // free resources
    vkDestroyShaderModule(VulkanEngine::get().getDevice(), shaderModule, nullptr);
    spvReflectDestroyShaderModule(&module);

    return newSubpass;
}

void ComputeEffect::execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize) {
    if (!mEnabled) {
        return;
    }

    for (unsigned int i = 0; i < mSubpasses.size(); i++) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipeline);

        DescriptorWriter writer;
        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, image.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                                                                   .getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

        // write image extent to push constants
        for (auto& pushInfo : mSubpasses[i].pushInfos) {
            if (pushInfo.name == "imageExtent") {
                *(VkExtent2D*)(mSubpasses[i].pushConstants + pushInfo.offset) = extent;
                break;
            }
        }

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
    for (auto& subpass : mSubpasses) {
        VkDevice device = VulkanEngine::get().getDevice();

        vkDestroyDescriptorSetLayout(device, subpass.descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(device, subpass.pipelineLayout, nullptr);
        vkDestroyPipeline(device, subpass.pipeline, nullptr);
    }
}

void ComputeEffect::drawGui() {
    if (ImGui::TreeNode(mName.c_str())) {
        ImGui::Checkbox("enable", &mEnabled);

        for (auto& subpass : mSubpasses) {
            for (auto& pushInfo : subpass.pushInfos) {
                if (pushInfo.name == "imageExtent") {
                    continue;
                }

                bool isVector = pushInfo.type & SPV_REFLECT_TYPE_FLAG_VECTOR;
                uint32_t count = isVector ? pushInfo.size / 4 : 1;

                if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_INT) {
                    if (count == 1) {
                        ImGui::InputInt(pushInfo.name.c_str(), (int*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 2) {
                        ImGui::InputInt2(pushInfo.name.c_str(), (int*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 3) {
                        ImGui::InputInt3(pushInfo.name.c_str(), (int*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 4) {
                        ImGui::InputInt4(pushInfo.name.c_str(), (int*)(subpass.pushConstants + pushInfo.offset));
                    }
                } else if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_FLOAT) {
                if (count == 1) {
                        ImGui::InputFloat(pushInfo.name.c_str(), (float*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 2) {
                        ImGui::InputFloat2(pushInfo.name.c_str(), (float*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 3) {
                        ImGui::InputFloat3(pushInfo.name.c_str(), (float*)(subpass.pushConstants + pushInfo.offset));
                    } else if (count == 4) {
                        ImGui::InputFloat4(pushInfo.name.c_str(), (float*)(subpass.pushConstants + pushInfo.offset));
                    }
                } else if (pushInfo.type & SPV_REFLECT_TYPE_FLAG_BOOL) {
                    ImGui::Checkbox(pushInfo.name.c_str(), (bool *)(subpass.pushConstants + pushInfo.offset));
                }
            }
        }

        ImGui::TreePop();
    }
}