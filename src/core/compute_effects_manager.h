#pragma once
#include "compute_effect.h"
#include "colors.h"

class ComputeEffectsManager {

public:
    ComputeEffectsManager(VulkanEngine& vkEngine);
    void loadEffect(const std::filesystem::path& path);
    void executeEffects(VkCommandBuffer commandBuffer, ComputeEffect::Context context);
    void drawGui(ImGuiContext& context);
    void loadEffects();

    void setProjectPath(const std::string& path) {
        mProjectPath = path;
    }

    void disableEffect(const std::string& name) {
        if (!mEffects.contains(name)) return;

        mEffects[name]->disable();
    }

    void enableEffect(const std::string& name) {
        if (!mEffects.contains(name)) return;

        mEffects[name]->enable();
    }

    void setValue(const std::string& effect, const std::string& name, float value) {
        if (!mEffects.contains(effect)) return;

        mEffects[effect]->setValue(name, value);
    }

private:
    void parseGlobalConfig();

    VulkanEngine& mVkEngine;
    std::string mProjectPath = ".";

    std::unordered_map<std::string, std::unique_ptr<ComputeEffect>> mEffects;
    std::vector<std::string> mEffectOrder;
};
