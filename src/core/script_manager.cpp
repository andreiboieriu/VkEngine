#include "script_manager.h"
#include "components/components.h"
#include "entity.h"
#include "sol/sol.hpp"
#include <GLFW/glfw3.h>
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
    // if (mIsInitialized) {
    //     return;
    // }

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
        sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
        sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
        sol::meta_function::multiplication, [](const glm::vec3& a, const float& b) { return a * b; }
    );

    mLua.new_usertype<glm::ivec2>(
        "IVec2", sol::constructors<glm::ivec2(), glm::ivec2(int, int)>(),
        "x", &glm::ivec2::x,
        "y", &glm::ivec2::y,
        sol::meta_function::addition, [](const glm::ivec2& a, const glm::ivec2& b) { return a + b; }
    );

    mLua.new_usertype<glm::quat>(
        "Quat",
        sol::constructors<
            glm::quat()
        >(),
        "w", &glm::quat::w,        // Quaternion scalar (w)
        "x", &glm::quat::x,        // Quaternion x component
        "y", &glm::quat::y,        // Quaternion y component
        "z", &glm::quat::z,         // Quaternion z component
        "ToEulerAngles", [](const glm::quat& q) -> glm::vec3 {
            // Convert quaternion to Euler angles (in radians)
            return glm::eulerAngles(q);  // Returns a glm::vec3 representing pitch, yaw, roll
        },
        "FromEulerAngles", [](glm::quat& q, const glm::vec3& euler) -> void {
            // Convert Euler angles (in radians) to quaternion
            q = glm::quat(euler);
        }
    );

    mLua.new_usertype<Transform>(
        "Transform", sol::constructors<Transform(), Transform(const Transform&)>(),
        "position", &Transform::position,
        "rotation", &Transform::rotation,
        "forward", &Transform::forward,
        "right", &Transform::right,
        "Translate", &Transform::translate,
        "Rotate", &Transform::rotate,
        "SetPosition", [](Transform& self, glm::vec3 position) -> void {
            self.position = position;
        },
        "SetRotation", [](Transform& self, glm::quat rotation) -> void {
            self.rotation = rotation;
        },
        "LookAt", [](Transform& self, glm::vec3 target) -> void {
            glm::vec3 direction = glm::normalize(target - self.position);
            self.rotation = glm::rotation(glm::vec3(0.f, 0.f, -1.f), direction);
        }
    );

    // add tables
    mLua["KeyCode"] = mLua.create_table_with(
        "W", glfwGetKeyScancode(GLFW_KEY_W),
        "A", glfwGetKeyScancode(GLFW_KEY_A),
        "S", glfwGetKeyScancode(GLFW_KEY_S),
        "D", glfwGetKeyScancode(GLFW_KEY_D),
        "Q", glfwGetKeyScancode(GLFW_KEY_Q),
        "E", glfwGetKeyScancode(GLFW_KEY_E),
        "Z", glfwGetKeyScancode(GLFW_KEY_Z),
        "C", glfwGetKeyScancode(GLFW_KEY_C),
        "Space", glfwGetKeyScancode(GLFW_KEY_SPACE)
    );

    mLua["Input"] = mLua.create_table_with(
        "GetKey", [&](int scancode) -> bool {
            InputState state = mInput.keyStates.at(scancode);
            return (state == InputState::HELD || state == InputState::PRESSED);
        }
    );

    mLua["Camera"] = mLua.create_table_with(
        "GetTransform", [&]() -> Transform& {
           return mCamera->getComponent<Transform>();
        }
    );

    mLua["Time"] = mLua.create_table_with(
        "deltaTime", 0.f
    );

    // mIsInitialized = true;
    fmt::println("Initialized lua state");
}

ScriptManager::ScriptManager() {
    initializeLuaState();
}

ScriptManager::~ScriptManager() {

}

void ScriptManager::loadScript(const std::string& filePath) {
    if (mLoadedScripts.contains(filePath)) {
        return;
    }

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
    Script& scriptComponent = entity.getComponent<Script>();

    scriptComponent.name = scriptName;
    scriptComponent.symbols = scriptData.symbols;
    scriptComponent.initialized = false;

    // create a new environment
    sol::environment& env = scriptComponent.env;
    env = sol::environment(mLua, sol::create, mLua.globals());

    // load bytecode into the new environment
    auto result = mLua.safe_script(scriptData.bytecode.as_string_view(), env);

    if (!result.valid()) {
        sol::error err = result;
        fmt::println("failed to bind script {}: {}", scriptName, err.what());
    }

    // set entity specific functions
    env.set_function("getTransform", &Entity::getComponent<Transform>, &entity);
}

void ScriptManager::update(float deltaTime, const Input& input, Entity *camera) {
    mLua["Time"]["deltaTime"] = deltaTime;
    mInput = input;
    mCamera = camera;
}

void ScriptManager::onUpdate(entt::registry& registry) {
    auto view = registry.view<Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        if (script.name == "") {
            continue;
        }

        sol::protected_function_result result = script.env["update"]();

        if (!result.valid()) {
            sol::error err = result;
            fmt::println("onUpdate function failed {}: {}", metadata.uuid, err.what());
        }
    }
}

void ScriptManager::onInit(entt::registry& registry) {
    auto view = registry.view<Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        if (script.name == "") {
            continue;
        }

        if (script.initialized) {
            continue;
        }

        sol::protected_function_result result = script.env["init"]();

        if (!result.valid()) {
            sol::error err = result;
            fmt::println("onInit function failed {}: {}", metadata.uuid, err.what());
        }

        script.initialized = true;
    }
}
