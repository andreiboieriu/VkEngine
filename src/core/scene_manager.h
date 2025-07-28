#pragma once

#include <filesystem>
#include <vk_scene.h>
#include "colors.h"

class VulkanEngine;

class SceneManager {

public:
    SceneManager(VulkanEngine& vkEngine);
    // ~SceneManager();

    void init();

    void loadAllScenes();
    void getEntryScene();
    void loadScene(const std::filesystem::path& filePath);
    void loadSceneAsync(const std::string& name);

    std::string getNewSceneName();

    void refreshScripts();

    void drawSceneAssets(ImGuiContext& context);

    void saveToFile();
    void restoreScenes(ImGuiContext& context);

    bool finishedLoadingScene() {
        return mFinishedLoadingScene;
    }

    bool isSceneLoaded(const std::string& scene) {
        return mLoadedScenes.contains(scene);
    }

    bool hasActiveScene() {
        return mActiveScene != "";
    }

    Scene3D& getActiveScene() {
        return *mLoadedScenes[mActiveScene];
    }

    std::string getActiveSceneName() {
        return mActiveScene;
    }

    void switchScene(const std::string& newScene, ImGuiContext& context) {
        mActiveScene = newScene;
        mFinishedLoadingScene = false;
        context.selectedEntity = "";
        context.changedScene = true;
        context.visitedScenes.insert(newScene);
    }

    void setProjectPath(const std::string& projectPath) {
        mProjectPath = projectPath;
    }

private:
    VulkanEngine& mVkEngine;

    std::unordered_map<std::string, std::unique_ptr<Scene3D>> mLoadedScenes;
    std::string mActiveScene = "";
    std::string mEntryScene = "";
    std::string mProjectPath = ".";

    bool mFinishedLoadingScene;
};
