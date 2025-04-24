#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <entt.hpp>
#include "entity.h"
#include "vk_window.h"

#include <components/components.h>

class ScriptManager {

public:
    ScriptManager();
    ~ScriptManager();

    void loadScript(const std::string& filePath);
    void bindScript(const std::string& scriptName, Entity& entity);

    void update(float deltaTime, const Input& input);

    void onStart();
    void onUpdate(entt::registry& registry);
    void onLateUpdate();
    void onDestroy();

private:

    struct ScriptData {
        sol::bytecode bytecode;
        std::unordered_map<std::string, sol::type> symbols;
    };

    void initializeLuaState();
    std::unordered_map<std::string, sol::type> getSymbols(const sol::bytecode& bytecode);

    sol::state mLua;
    // inline static bool mIsInitialized = false;

    Input mInput;

    std::unordered_map<std::string, ScriptData> mLoadedScripts;
};
