#include "scene_manager.h"
#include "nlohmann/json.hpp"
#include "vk_scene.h"

#include <fstream>
#include <thread>
#include "vk_engine.h"

SceneManager::SceneManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {

}

void SceneManager::init() {
    getEntryScene();

    if (mProjectPath != ".") {
        loadAllScenes();
    } else {
        loadScene(mProjectPath + "/assets/scenes/" + mEntryScene);
    }

    mActiveScene = mEntryScene;
}

void SceneManager::getEntryScene() {
    mEntryScene = mVkEngine.getEntryPoint();
}

void SceneManager::loadScene(const std::filesystem::path& filePath) {
    fmt::println("Loading scene: {}", filePath.filename().c_str());
    mLoadedScenes[filePath.filename()] = std::make_unique<Scene3D>(filePath, mVkEngine);
}

void SceneManager::loadSceneAsync(const std::string& scene) {
    fmt::println("Loading scene (async): {}", scene);
    std::filesystem::path filePath = mProjectPath + "/assets/scenes/" + scene;

    if (!std::filesystem::exists(filePath)) {
        fmt::println("Error: couldn't find file {}", filePath.c_str());
        return;
    }

    if (mLoadedScenes.contains(scene)) {
        mFinishedLoadingScene = true;
        return;
    }

    std::thread([scene, filePath, this](){
        mFinishedLoadingScene = false;
        mLoadedScenes[scene] = std::make_unique<Scene3D>(filePath, mVkEngine);
        mFinishedLoadingScene = true;
    }).detach();
}

void SceneManager::loadAllScenes() {
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(mProjectPath + "/assets/scenes")) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        if (entry.path().filename() == "config.json") continue;

        loadScene(mProjectPath + "/assets/scenes/" + entry.path().filename().string());
    }
}

void SceneManager::drawSceneAssets(ImGuiContext& context) {
    const uint32_t maxTextLength = 15;

    ImGui::PushFont(context.font12);
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    auto drawList = ImGui::GetWindowDrawList();
    auto& jsonIcon = *mVkEngine.getAssetManager().getImGuiImage("json-icon.png");

    ImVec2 p0 = ImGui::GetCursorScreenPos();

    uint32_t itemsPerRow = 9;
    uint32_t row = 0;
    uint32_t col = 0;

    ImVec2 spacing = context.spacing;
    float iconWidth = (1000.f - (itemsPerRow + 1) * spacing.x) / itemsPerRow;
    ImVec2 iconSize = ImVec2(iconWidth, iconWidth);
    float textHeight = ImGui::CalcTextSize("dummy.hdr").y;

    for (auto& [k, v] : mLoadedScenes) {
        ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y));
        ImVec2 max = ImVec2(min.x + iconSize.x, min.y + iconSize.y);

        std::string text = k;
        if (text.length() > maxTextLength) {
            text = text.substr(0, maxTextLength - 3) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float textOffset = (iconSize.x - textSize.x) * 0.5f;

        ImGui::SetCursorScreenPos(min);

        ImGui::BeginGroup();
        ImGui::Image(jsonIcon.id, iconSize);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - textHeight);
        ImGui::Text(text.c_str());
        ImGui::EndGroup();

        static double lastClickTime = 0.f;

        ImGui::SetCursorScreenPos(min);
        std::string buttonName = "button" + k;
        if (ImGui::InvisibleButton(buttonName.c_str(), iconSize)) {
            double clickTime = ImGui::GetTime();

            if (clickTime - lastClickTime < 0.3f) {
                if (k != mActiveScene) {
                    switchScene(k, context);
                }
            }

            lastClickTime = clickTime;
        }

        if (++col == itemsPerRow) {
            row++;
            col = 0;
        }
    }

    auto& plusIcon = *mVkEngine.getAssetManager().getImGuiImage("plus.png");
    ImVec2 plusSize = ImVec2(20, 20);
    ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y) + (iconSize.y + spacing.y - plusSize.y - 2 * ImGui::GetStyle().FramePadding.y) / 2);

    if (row == 0 && col == 0) {
        min.x = p0.x + (iconSize.x - plusSize.x - 2 * ImGui::GetStyle().FramePadding.x) / 2;
    }

    ImGui::SetCursorScreenPos(min);

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);

    if (ImGui::ImageButton("AddScene", plusIcon.id, plusSize)) {
        std::string sceneName = getNewSceneName();
        mLoadedScenes[sceneName] = std::make_unique<Scene3D>(mVkEngine);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopFont();
}

std::string SceneManager::getNewSceneName() {
    std::string baseName = "NewScene";
    std::string name = baseName;
    int curr = 1;

    while (mLoadedScenes.contains(name + ".json")) {
        name = baseName + std::to_string(curr++);
    }

    return name + ".json";
}

void SceneManager::saveToFile() {
    for (auto& [k, v] : mLoadedScenes) {
        fmt::println("Saving scene: {}", k.c_str());
        v->saveToFile(mProjectPath + "/assets/scenes/" + k);
    }
}

void SceneManager::restoreScenes(ImGuiContext& context) {
    for (auto& [k, v] : mLoadedScenes) {
        if (context.visitedScenes.contains(k))
            v->loadFromFile(mProjectPath + "/assets/scenes/" + k);
    }

    mActiveScene = context.firstScene;

    context.firstScene = "";
    context.visitedScenes.clear();
}

void SceneManager::refreshScripts() {
    for (auto& [k, v] : mLoadedScenes) {
        v->refreshScripts();
    }
}
