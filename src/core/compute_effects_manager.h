#pragma once
#include "compute_effect.h"

class ComputeEffectsManager {

public:
    ComputeEffectsManager(VulkanEngine& vkEngine);
    void loadEffect(const std::string& name);
    void executeEffects(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent);

private:
    void loadDefaultEffects();

    VulkanEngine& mVkEngine;

    std::unordered_map<std::string, std::unique_ptr<ComputeEffect>> mEffects;
};
