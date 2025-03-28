#pragma once

#include "volk.h"
#include "vk_types.h"
#include <span>
#include <string>

#include "spirv_reflect.h"

// forward reference
class VulkanEngine;

class ComputeEffect {

public:
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

    ComputeEffect(std::string name, VulkanEngine& vkEngine);
    ComputeEffect(std::string name, const std::vector<ComputeEffect::EditableSubpassInfo>& info, VulkanEngine& vkEngine);

    ~ComputeEffect();

    virtual void execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, bool synchronize);
    virtual void drawGui();

protected:
    void createPipelineLayout();
    void synchronizeWithCompute(VkCommandBuffer commandBuffer);

    void load(std::string name);
    void setSubpassInfo(const std::vector<ComputeEffect::EditableSubpassInfo>& info);
    void addSubpass(std::string path);

    void freeResources();

    VulkanEngine& mVkEngine;

    std::vector<Subpass> mSubpasses;

    // TODO: improve later
    bool mUseBufferImage = false;
    AllocatedImage mBufferImage;

    std::vector<VkImageView> mBufferImageMipViews;

    std::string mName;
    bool mEnabled = true;
};
