#include "compute_effects_manager.h"
#include <imgui.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

ComputeEffectsManager::ComputeEffectsManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
}

void ComputeEffectsManager::loadEffects() {
    static const std::filesystem::path root(mProjectPath + "/assets/compute_effects/");
    static const std::set<std::string> reservedNames{
        "textures"
    };

    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (!entry.is_directory()) {
            continue;
        }

        if (reservedNames.contains(entry.path().stem())) {
            continue;
        }

        loadEffect(entry);
    }

    parseGlobalConfig();
}

void ComputeEffectsManager::parseGlobalConfig() {
    static const std::filesystem::path path(mProjectPath + "/assets/compute_effects/global_config.json");

    if (!std::filesystem::exists(path)) {
        fmt::print("Error: Missing compute effects global config\n");
        return;
    }

    std::ifstream file(path);

    if (!file.is_open()) {
        fmt::print("Error opening global config file: {}\n", path.string());
        return;
    }

    nlohmann::json configJson;
    file >> configJson;

    if (!configJson.contains("effect_order") || !configJson["effect_order"].is_array()) {
        fmt::println("Error: 'effect_order' is missing or not an array in the JSON.");
        return;
    }

    for (const auto& entry : configJson["effect_order"]) {
        if (!entry.is_string()) {
            fmt::println("Error: entries in effect order should be strings");
            return;
        }

        mEffectOrder.push_back(entry);
    }
}

void ComputeEffectsManager::loadEffect(const std::filesystem::path& path) {
    fmt::println("Loading post-processing effect: {}", path.stem().c_str());

    std::string name = path.stem();

    if (mEffects.contains(name)) {
        fmt::println("Failed to load compute effect {}: already exists", name);
        return;
    }

    mEffects[name] = std::make_unique<ComputeEffect>(path, mVkEngine);
}

void ComputeEffectsManager::executeEffects(VkCommandBuffer commandBuffer, ComputeEffect::Context context) {
    for (uint32_t i = 0; i < mEffectOrder.size(); i++) {
        bool sync = i != mEffectOrder.size() - 1;

        mEffects[mEffectOrder[i]]->execute(commandBuffer, context, sync);
    }
}

void ComputeEffectsManager::drawGui(ImGuiContext& context) {
    ImGui::PushStyleColor(ImGuiCol_Header, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HOVERED_BUTTON_COLOR);

    for (auto& [k, v] : mEffects) {
        v->drawGui(context);
    }

    ImGui::PopStyleColor(3);
}
