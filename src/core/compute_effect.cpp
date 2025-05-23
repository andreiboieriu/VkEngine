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
#include <nlohmann/json.hpp>

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

ComputeEffect::ComputeEffect(const std::filesystem::path& path, VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    load(path);
}

ComputeEffect::~ComputeEffect() {
    freeResources();
}

void ComputeEffect::load(const std::filesystem::path& path) {
    mName = path.stem();

    int currPass = 0;

    while (true) {
        // check if pass exists
        std::string subpassPath = path.string() + "/pass" + std::to_string(currPass++) + ".comp.spv";

        if (!std::filesystem::exists(subpassPath)) {
            fmt::println("Compute effect {} not found", subpassPath);
            break;
        }

        // create new subpass
        addSubpass(subpassPath);
    };

    parseConfig(path.string() + "/config.json");
}

void ComputeEffect::parseConfig(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        fmt::println("Missing config file for compute effect {}", mName);
        return;
    }

    std::ifstream file(path);

    if (!file.is_open()) {
        fmt::print("Error opening config file: {}\n", path.string());
        return;
    }

    nlohmann::json effectJson;
    file >> effectJson;

    if (!effectJson.contains("subpasses") || !effectJson["subpasses"].is_array()) {
        fmt::println("Error: 'subpasses' is missing or not an array in the JSON.");
        return;
    }

    for (const auto& subpassJson : effectJson["subpasses"]) {
        if (!subpassJson.contains("index") || !subpassJson["index"].is_number_integer()) {
            fmt::println("Error: Subpass index is missing or not an integer.");
            return;
        }

        int index = subpassJson["index"];

        if (index >= mSubpasses.size() || index < 0) {
            fmt::println("Error: Subpass index is out of bounds");
        }

        auto& subpass = mSubpasses[index];

        if (subpassJson.contains("inheritFrom")) {
            int inheritFrom = subpassJson["inheritFrom"];

            if (inheritFrom >= index) {
                fmt::print("Error: inheritFrom value must be lower than the current index.");
                return;
            }

            subpass.editableInfo = mSubpasses[inheritFrom].editableInfo;
        }

        if (subpassJson.contains("values")) {
            for (const auto& [k, v] : subpassJson["values"].items()) {
                int valueIndex = std::stoi(k);

                // make sure to not overwrite other fields in push constants
                if (valueIndex > 4 || valueIndex < 0) {
                    fmt::println("Error: value entry is out of bounds");
                }

                static const std::set<std::string> possibleFields{
                    "x", "y", "z", "w"
                };

                for (const auto& [field, fieldJson] : v.items()) {
                    if (!possibleFields.contains(field)) {
                        fmt::print("Error: illegal value field {}\n", field);
                        return;
                    }

                    if (!fieldJson.contains("default")) {
                        fmt::print("Error: field doesn't contain a default value\n");
                        return;
                    }

                    if (!fieldJson["default"].is_number()) {
                        fmt::print("Error: default value is not a number\n");
                        return;
                    }

                    float value = fieldJson["default"];
                    uint32_t offset = sizeof(glm::vec4) * valueIndex;

                    if (field == "x") {
                        subpass.editableInfo.pushConstants[valueIndex].x = value;
                    } else if (field == "y") {
                        subpass.editableInfo.pushConstants[valueIndex].y = value;
                        offset += sizeof(float);
                    } else if (field == "z") {
                        subpass.editableInfo.pushConstants[valueIndex].z = value;
                        offset += 2 * sizeof(float);
                    } else if (field == "w") {
                        subpass.editableInfo.pushConstants[valueIndex].w = value;
                        offset += 3 * sizeof(float);
                    }

                    if (fieldJson.contains("name")) {
                        std::string name = fieldJson["name"];

                        fmt::print("Set offset {} for field {}", offset, name);

                        // check if another field with the same offset exists, and delete it
                        std::string foundKey = "";
                        for (const auto& [fieldName, fieldOffset] : subpass.editableInfo.namedValues) {
                            if (fieldOffset == offset) {
                                foundKey = fieldName;
                                break;
                            }
                        }

                        if (foundKey != "") {
                            subpass.editableInfo.namedValues.erase(foundKey);
                        }

                        subpass.editableInfo.namedValues[name] = offset;
                    }
                }
            }
        }

        // Access "dispatch" and ensure it has the required fields
        if (subpassJson.contains("dispatch")) {
            if (subpassJson["dispatch"].contains("useScreenSize")) {
                if (!subpassJson["dispatch"]["useScreenSize"].is_boolean()) {
                    fmt::println("Error: useScreenSize field should be true/false");
                    return;
                }

                subpass.editableInfo.dispatchSize.useScreenSize = subpassJson["dispatch"]["useScreenSize"];
            }

            if (subpassJson["dispatch"].contains("screenSizeMultiplier")) {
                if (!subpassJson["dispatch"]["screenSizeMultiplier"].is_number()) {
                    fmt::println("Error: useScreenSize field should be a number");
                    return;
                }

                float multiplier = subpassJson["dispatch"]["screenSizeMultiplier"];

                if (multiplier < 0.f || multiplier > 1.f) {
                    fmt::println("Error: screenSizeMultiplier should be between 0 and 1");
                    return;
                }

                subpass.editableInfo.dispatchSize.useScreenSize = multiplier;
            }

            if (subpassJson["dispatch"].contains("customSize")) {
                const auto& customSizeJson = subpassJson["dispatch"]["customSize"];

                if (!customSizeJson.contains("x") || !customSizeJson.contains("y")) {
                    fmt::println("Error: custom size should have both x and y fields");
                    return;
                }

                if (!customSizeJson["x"].is_number() || !customSizeJson["y"].is_number()) {
                    fmt::println("Error: custom size values should be numbers");
                    return;
                }

                float x = customSizeJson["x"];
                float y = customSizeJson["y"];

                subpass.editableInfo.dispatchSize.customSize = glm::vec2(x, y);
            }
        }

        // Here you can print or process the subpass data as needed, or leave empty.
        fmt::print("Subpass {} parsed successfully\n", index);
    }

    fmt::print("{} config parsed successfully\n", path.stem().string());
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

void ComputeEffect::execute(VkCommandBuffer commandBuffer, Context context, bool sync) {
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
                    context.colorImage.imageView,
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? context.linearSampler : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else if (binding.name.find("bufferImageMip") != std::string::npos) {
                uint32_t mipLevel = std::stoi(binding.name.substr(std::string("bufferImageMip").size()));

                writer.writeImage(
                    j,
                    context.bufferImage.mipViews[mipLevel],
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? context.linearSampler : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else if (context.additionalImages.contains(binding.name)) {
                writer.writeImage(
                    j,
                    context.additionalImages[binding.name].imageView,
                    (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? context.linearSampler : VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL,
                    binding.descriptorType);
            } else {
                fmt::println("Error when executing compute effect {}, undefined binding {}", mName, binding.name);
            }
        }

        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mSubpasses[i].pipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

        // write push constants (image and buffer extent)
        mSubpasses[i].editableInfo.pushConstants[4].x = context.screenSize.width;
        mSubpasses[i].editableInfo.pushConstants[4].y = context.screenSize.height;
        mSubpasses[i].editableInfo.pushConstants[4].z = context.colorImage.imageExtent.width;
        mSubpasses[i].editableInfo.pushConstants[4].w = context.colorImage.imageExtent.height;

        vkCmdPushConstants(commandBuffer, mSubpasses[i].pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, mSubpasses[i].pushConstantsSize, mSubpasses[i].editableInfo.pushConstants.data());

        glm::vec2 dispatchSize;

        if (mSubpasses[i].editableInfo.dispatchSize.useScreenSize) {
            dispatchSize = glm::vec2(context.screenSize.width, context.screenSize.height);
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

    if (sync) {
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
}

void ComputeEffect::drawGui() {
    if (ImGui::TreeNode(mName.c_str())) {
        ImGui::Checkbox("enable", &mEnabled);

        for (int i = 0; i < mSubpasses.size(); i++) {
            if (ImGui::TreeNode(("subpass_" + std::to_string(i)).c_str())) {
                for (auto& [k, v] : mSubpasses[i].editableInfo.namedValues) {
                    ImGui::InputFloat(k.c_str(), (float*)((uint8_t*)&mSubpasses[i].editableInfo.pushConstants + v));
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
