#include "script_manager.h"
#include "ecs_components/components.h"
#include "entity.h"
#include "sol/sol.hpp"
#include <fmt/core.h>

// extracts global vars names from bytecode
std::unordered_map<std::string, sol::type> ScriptManager::getSymbols(const sol::bytecode& bytecode) {
    // create a temporary sol::environment
    sol::environment tempEnv(mLua, sol::create, mLua.globals());

    // load bytecode into the environment
    auto result = mLua.safe_script(bytecode.as_string_view(), tempEnv);

    if (!result.valid()) {
        fmt::println("failed to extract global vars from bytecode");
        return std::unordered_map<std::string, sol::type>();
    }

    // extract symbols from environment
    std::unordered_map<std::string, sol::type> symbols;

    for (const auto& pair : tempEnv) {
        sol::object key = pair.first;
        sol::object value = pair.second;

        if (key.is<std::string>()) {
            symbols[key.as<std::string>()] = value.get_type();

            if (value.is<sol::function>()) {
                fmt::println("function found: {}", key.as<std::string>());
            }
        }
    }

    for (auto& [symbol, type] : symbols) {
        fmt::println("symbol: {}", symbol);
    }

    return symbols;
}

void ScriptManager::initializeLuaState() {
    if (mIsInitialized) {
        return;
    }

    // open lua libraries
    mLua.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::table,
        sol::lib::io
    );

    // add usertypes
    mLua.new_usertype<glm::vec3>(
        "Vec3", sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
        "x", &glm::vec3::x,
        "y", &glm::vec3::y,
        "z", &glm::vec3::z,
        sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; }
    );

    mLua.new_usertype<Transform>(
        "Transform", sol::constructors<Transform()>(),
        "position", &Transform::position
    );

    mIsInitialized = true;
    fmt::println("Initialized lua state");
}

ScriptManager::ScriptManager(entt::registry& registry) : mRegistry(registry) {
    initializeLuaState();
}

ScriptManager::~ScriptManager() {

}

void ScriptManager::loadScript(const std::string& filePath) {
    // load script from file without executing it
    sol::load_result loadResult = mLua.load_file(filePath);

    if (!loadResult.valid()) {
        sol::error err = loadResult;
        fmt::println("failed to load script {}: {}", filePath, err.what());
        return;
    }

    // convert the loaded file into bytecode
    sol::protected_function target = loadResult.get<sol::protected_function>();
    sol::bytecode targetBytecode = target.dump();

    // save the bytecode
    mLoadedScripts[filePath] = {targetBytecode, getSymbols(targetBytecode)};
}

void ScriptManager::bindScript(const std::string& scriptName, Entity& entity) {
    // check if script is loaded
    if (!mLoadedScripts.contains(scriptName)) {
        fmt::println("Failed to bind script {}: no such script has been loaded", scriptName);
        return;
    }

    ScriptData& scriptData = mLoadedScripts[scriptName];

    // set script path and global vars symbols
    entity.getComponent<Script>().path = scriptName;
    entity.getComponent<Script>().symbols = scriptData.symbols;

    // create a new environment
    sol::environment& env = entity.getComponent<Script>().env;
    env = sol::environment(mLua, sol::create, mLua.globals());

    // load bytecode into the new environment
    auto result = mLua.safe_script(scriptData.bytecode.as_string_view(), env);

    if (!result.valid()) {
        sol::error err = result;
        fmt::println("failed to bind script {}: {}", scriptName, err.what());
    }

    env.set_function("getTransform", &Entity::getComponent<Transform>, &entity);
}

void ScriptManager::onUpdate(float dt) {
    auto view = mRegistry.view<Script>();

    for (auto [entity, script] : view.each()) {
        if (script.path == "") {
            continue;
        }

        script.env["update"](dt);
        script.env["cock"]();
    }
}
