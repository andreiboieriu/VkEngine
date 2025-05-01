#include "compute_effects_manager.h"

ComputeEffectsManager::ComputeEffectsManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    loadDefaultEffects();
}

void ComputeEffectsManager::loadDefaultEffects() {
    // load all other resources
    {
        ComputeEffect::EditableSubpassInfo info;
        info.pushConstants[0].x = 0.0312f; // fixedThreshold
        info.pushConstants[0].y = 0.063f; // relativeThreshold
        info.pushConstants[0].z = 0.75f; // pixelBlendStrength
        info.pushConstants[0].w = 2; // quality

        info.dispatchSize.useScreenSize = true;
        info.dispatchSize.screenSizeMultiplier = 1.0f;

        mEffects["fxaa"] = std::make_unique<ComputeEffect>("fxaa", std::vector<ComputeEffect::EditableSubpassInfo>{info, info}, mVkEngine, true);
    }

    {
        ComputeEffect::EditableSubpassInfo info;
        info.pushConstants[0].x = 1.0f; // exposure

        info.dispatchSize.useScreenSize = true;
        info.dispatchSize.screenSizeMultiplier = 1.0f;

        mEffects["tone_mapping"] = std::make_unique<ComputeEffect>("tone_mapping", std::vector<ComputeEffect::EditableSubpassInfo>{info}, mVkEngine, true);
    }

    {
        ComputeEffect::EditableSubpassInfo info;
        info.pushConstants[0].x = 0.005f; // filterRadius
        info.pushConstants[0].y = 0.04f; // strength

        info.dispatchSize.useScreenSize = true;
        info.dispatchSize.screenSizeMultiplier = 1.f;

        std::vector<ComputeEffect::EditableSubpassInfo> infoVec;

        for (int i = 1; i < 6; i++) {
            info.dispatchSize.screenSizeMultiplier = 1.f / (1 << i);
            infoVec.push_back(info);
        }

        for (int i = 4; i >= 0; i--) {
            info.dispatchSize.screenSizeMultiplier = 1.f / (1 << i);
            infoVec.push_back(info);
        }

        mEffects["bloom"] = std::make_unique<ComputeEffect>("bloom", infoVec, mVkEngine, true);
    }
}

void ComputeEffectsManager::loadEffect(const std::string& name) {
    if (mEffects.contains("name")) {
        fmt::println("Failed to load compute effect {}: already exists", name);
        return;
    }

    mEffects[name] = std::make_unique<ComputeEffect>(name, mVkEngine);
}

void ComputeEffectsManager::executeEffects(VkCommandBuffer commandBuffer, const AllocatedImage& image, VkExtent2D extent, VkSampler sampler) {
    mEffects["bloom"]->execute(commandBuffer, image, extent, true, sampler);
    mEffects["tone_mapping"]->execute(commandBuffer, image, extent, true, sampler);
    mEffects["fxaa"]->execute(commandBuffer, image, extent, false, sampler);
}
