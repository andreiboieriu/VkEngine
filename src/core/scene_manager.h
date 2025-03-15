#pragma once

#include "vk_scene.h"

class SceneManager {

public:
    SceneManager();
    ~SceneManager();



private:

    std::unordered_map<std::string, std::unique_ptr<Scene3D>> mLoadedScenes;
    std::string mActiveSceneName = "";
};
