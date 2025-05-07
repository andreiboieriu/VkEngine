#include "compute_effects_manager.h"
#include <imgui.h>

ComputeEffectsManager::ComputeEffectsManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    loadDefaultEffects();
}

void ComputeEffectsManager::loadDefaultEffects() {
    mEffects["fxaa"] = std::make_unique<ComputeEffect>("fxaa", mVkEngine, true);
    mEffects["tone_mapping"] = std::make_unique<ComputeEffect>("tone_mapping", mVkEngine, true);
    mEffects["bloom"] = std::make_unique<ComputeEffect>("bloom", mVkEngine, true);
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

void ComputeEffectsManager::drawGui() {
    if (ImGui::TreeNode("Compute Effects")) {
        for (auto& [k, v] : mEffects) {
            v->drawGui();
        }

        ImGui::TreePop();
    }
}
