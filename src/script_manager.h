#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <entt.hpp>
#include "entity.h"
#include "vk_window.h"

class ScriptManager {

public:
    ScriptManager(entt::registry& registry);
    ~ScriptManager();

    void loadScript(const std::string& filePath);
    void bindScript(const std::string& scriptName, Entity& entity);

    void update();

    void onStart();
    void onUpdate();
    void onLateUpdate();
    void onDestroy();

private:

    struct ScriptData {
        sol::bytecode bytecode;
        std::unordered_map<std::string, sol::type> symbols;
    };

    void initializeLuaState();
    std::unordered_map<std::string, sol::type> getSymbols(const sol::bytecode& bytecode);

    inline static sol::state mLua;
    inline static bool mIsInitialized = false;

    entt::registry& mRegistry;
    const Input& mInput;

    std::unordered_map<std::string, ScriptData> mLoadedScripts;
};
