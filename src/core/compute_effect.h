#pragma once

#include "volk.h"
#include "vk_types.h"
#include <span>
#include <string>
#include <filesystem>

#include "spirv_reflect.h"

// forward reference
class VulkanEngine;

class ComputeEffect {

public:
    struct Context {
        const AllocatedImage& colorImage;
        const AllocatedImage& depthImage;
        const AllocatedImage& bufferImage;
        VkExtent2D screenSize;
        VkSampler linearSampler;

        std::unordered_map<std::string, AllocatedImage>& additionalImages;
    };

    struct PushConstantLocation {
        uint32_t subpass;
        uint32_t offset;
    };

    struct BindingData {
        std::string name;
        VkDescriptorType descriptorType;
    };

    struct DispatchSize {
        bool useScreenSize = true;
        float screenSizeMultiplier = 1.0f; // defaults to full screen

        glm::vec2 customSize; // used size if useScreenSize = false
    };

    struct EditableSubpassInfo {
        std::array<glm::vec4, 8> pushConstants;
        DispatchSize dispatchSize;

        std::unordered_map<std::string, uint32_t> namedValues; // the value represents the offset from the start of push constants
    };

    struct Subpass {
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        VkDescriptorSetLayout descriptorSetLayout;

        static constexpr size_t pushConstantsSize = 80;

        std::vector<BindingData> bindings;
        EditableSubpassInfo editableInfo;
        glm::vec3 localSize;
    };

    ComputeEffect(const std::filesystem::path& path, VulkanEngine& vkEngine);
    ~ComputeEffect();

    void execute(VkCommandBuffer commandBuffer, Context context, bool sync);
    void drawGui();

protected:
    void createPipelineLayout();
    void synchronizeWithCompute(VkCommandBuffer commandBuffer);

    void load(const std::filesystem::path& path);
    void parseConfig(const std::filesystem::path& path);
    void addSubpass(const std::filesystem::path& path);

    void freeResources();

    VulkanEngine& mVkEngine;

    std::vector<Subpass> mSubpasses;

    std::string mName;
    bool mEnabled = true;
};
