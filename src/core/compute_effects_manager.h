#pragma once
#include "compute_effect.h"

class ComputeEffectsManager {

public:
    ComputeEffectsManager(VulkanEngine& vkEngine);
    void loadEffect(const std::filesystem::path& path);
    void executeEffects(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, VkSampler sampler);
    void drawGui();

private:
    void loadEffects();
    void parseGlobalConfig();

    VulkanEngine& mVkEngine;

    std::unordered_map<std::string, std::unique_ptr<ComputeEffect>> mEffects;
    std::vector<std::string> mEffectOrder;
};
