#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <entt.hpp>
#include "entity.h"
#include "vk_window.h"
#include "colors.h"

#include <components/components.h>

class VulkanEngine;
class SceneManager;

class ScriptManager {

public:
    ScriptManager(VulkanEngine& vkEngine);
    ~ScriptManager();

    void loadScript(const std::string& name);
    void bindScript(const std::string& scriptName, Entity& entity);

    void refreshScripts();

    void update(float deltaTime, const Input& input);

    void onInit(entt::registry& registry);
    void onUpdate(entt::registry& registry);
    void onLateUpdate(entt::registry& registry);
    void onDestroy(entt::registry& registry);
    void onCollision(entt::registry& registry);

    void setProjectPath(const std::string& projectPath) {
        mProjectPath = projectPath;
    }

    void loadAllScripts();
    void drawScriptAssets(ImGuiContext& context);

private:

    std::string getNewScriptName();
    struct ScriptData {
        sol::bytecode bytecode;
        std::unordered_map<std::string, sol::type> symbols;
    };

    void initializeLuaState();
    std::unordered_map<std::string, sol::type> getSymbols(const sol::bytecode& bytecode);

    sol::state mLua;

    Input mInput;

    VulkanEngine& mVkEngine;
    SceneManager& mSceneManager;

    std::unordered_map<std::string, ScriptData> mLoadedScripts;
    std::string mProjectPath = ".";
};
