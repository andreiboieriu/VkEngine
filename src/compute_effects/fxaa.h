#pragma once

#include "volk.h"
#include "vk_types.h"

// TODO: generalize

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
};