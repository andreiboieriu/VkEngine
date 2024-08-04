#pragma once

#include "volk.h"
#include "vk_types.h"

class Fxaa {

public:
    struct PushConstants {
        glm::ivec2 imageExtent;
        float fixedThreshold;
        float relativeThreshold;
        float pixelBlendStrength;
        int quality;
    };

    struct Subpass {
        VkPipeline pipeline;
    };

    Fxaa();
    ~Fxaa();

    void execute(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent);
    void drawGui();

private:
    void createPipelineLayout();
    void addSubpass(std::string_view shaderPath);
    void freeResources();

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    std::vector<Subpass> mSubpasses;
    PushConstants mPushConstants;

    std::vector<VkWriteDescriptorSet> mDescriptorWrites;

    bool mEnabled;

    // const float fixedThresholdValues[3] = {
    //     0.0312,
    //     0.0625,
    //     0.0833
    // };

    // int selectedFixedThreshold = 0;

    // const float relativeThresholdValues[3] = {
    //     0.0312,
    //     0.0625,
    //     0.0833
    // };

    // int selectedRelativeThreshold = 0;
};