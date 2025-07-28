#include "script_manager.h"
#include "components/components.h"
#include "entity.h"
#include "sol/sol.hpp"
#include <GLFW/glfw3.h>
#include <fmt/core.h>
#include "vk_engine.h"
#include <unistd.h>

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
                // fmt::println("function found: {}", key.as<std::string>());
            }
        }
    }

    for (auto& [symbol, type] : symbols) {
        // fmt::println("symbol: {}", symbol);
    }

    return symbols;
}

void ScriptManager::initializeLuaState() {
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
        sol::meta_function::multiplication, [](const glm::vec3& a, const float& b) { return a * b; },
        "Cross", [](const glm::vec3& a, const glm::vec3& b) {
            return glm::cross(a, b);
        },
        "Normalize", [](const glm::vec3& a) {
            return glm::normalize(a);
        },
        "Distance", [](const glm::vec3& a, const glm::vec3& b) {
            return glm::distance(a, b);
        }
    );

    mLua.new_usertype<glm::vec2>(
        "Vec2", sol::constructors<glm::vec2(), glm::vec2(float, float)>(),
        "x", &glm::vec2::x,
        "y", &glm::vec2::y,
        sol::meta_function::addition, [](const glm::vec2& a, const glm::vec2& b) { return a + b; },
        sol::meta_function::subtraction, [](const glm::vec2& a, const glm::vec2& b) { return a - b; },
        sol::meta_function::multiplication, [](const glm::vec2& a, const float& b) { return a * b; }
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
            return glm::degrees(glm::eulerAngles(q));
        },
        "FromEulerAngles", [](glm::quat& q, const glm::vec3& euler) -> void {
            q = glm::quat(glm::radians(euler));
        },
        "Inverse", [](const glm::quat& q) { return glm::inverse(q); },
        sol::meta_function::multiplication, sol::overload(
            [](const glm::quat& q1, const glm::quat& q2) { return q1 * q2; },
            [](const glm::quat& q, const glm::vec3& v) { return q * v; }
        ),
        "FromAxisAngle", [](const glm::vec3& axis, float angle_deg) {
            return glm::angleAxis(glm::radians(angle_deg), glm::normalize(axis));
        },
        "Dot", [](const glm::quat& q1, const glm::quat& q2) { return glm::dot(q1, q2); },
        "Slerp", [](const glm::quat& q1, const glm::quat& q2, float t) { return glm::slerp(q1, q2, t); },
        "LookAt", sol::overload(
            // Version 1: Takes the camera position, the target position, and a custom up vector.
            [](const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
                // glm::quatLookAt is the robust, correct function for this job.
                // It creates a rotation that looks along the direction vector
                // while keeping the orientation aligned with the 'up' vector.
                return glm::quatLookAt(glm::normalize(center - eye), up);
            },
            // Version 2: A simpler overload that assumes a default world 'up' vector of (0, 1, 0).
            // This is the one you will use most often.
            [](const glm::vec3& eye, const glm::vec3& center) {
                const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
                return glm::quatLookAt(glm::normalize(center - eye), worldUp);
            }
        )
    );

    mLua.new_usertype<Transform>(
        "Transform", sol::no_constructor,
        "position", &Transform::position,
        "rotation", &Transform::rotation,
        "scale", &Transform::scale,
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
            self.rotation = glm::quatLookAt(direction, glm::vec3(0.f, 1.f, 0.f));
        }
    );

    mLua.new_usertype<Camera>(
        "Camera", sol::no_constructor,
        "fov", &Camera::fov,
        "near", &Camera::near,
        "far", &Camera::far
    );

    mLua.new_usertype<GLTF>(
        "GLTF", sol::no_constructor,
        "name", &GLTF::name,
        "SetGLTF", [&](GLTF& self, std::string name) {
            self.name = name;

            if (name != "None")
                self.gltf = mVkEngine.getAssetManager().getGltf(name);
        }
    );

    mLua.new_usertype<Sprite>(
        "Sprite", sol::no_constructor,
        "name", &Sprite::name,
        "SetSprite", [&](Sprite& self, std::string name) {
            self.name = name;

            if (name != "None")
                self.image = mVkEngine.getAssetManager().getImage(name);
        }
    );

    mLua.new_usertype<SphereCollider>(
        "SphereCollider", sol::no_constructor,
        "radius", &SphereCollider::radius
    );

    mLua.new_usertype<Scene3D>(
        "Scene", sol::no_constructor,
        "CreateEntity", [&](Scene3D& self, std::string name, std::string parent) {
            Entity* newEntity = self.createEntity(name);
            Entity* parentEntity = self.getEntity(parent);

            if (parentEntity) {
                newEntity->setParent(self.getEntity(parent));
                parentEntity->addChild(newEntity);
            }

            return std::ref(*newEntity);
        },
        "GetEntity", [&](Scene3D& self, std::string name) {
            return std::ref(*self.getEntity(name));
        },
        "DestroyEntity", [&](Scene3D& self, std::string name) {
            self.getEntity(name)->addComponent<Destroy>();
        }
    );

    mLua.new_usertype<Entity>(
        "Entity", sol::no_constructor,
        "GetUUID", &Entity::getUUID,
        "GetParent", &Entity::getParent,
        "GetChildren", &Entity::getChildren,
        "HasParent", &Entity::hasParent,
        "AddComponent", [&](Entity& self, std::string name, sol::this_state s) -> sol::object {
            sol::state_view lua(s);
            sol::object result = sol::make_object(lua, sol::nil);

            SerializableComponents::forEach([&](auto type) {
                using T = typename decltype(type)::type;
                auto typeName = getTypeName<T>();

                if (typeName == name) {
                    self.addComponent<T>();
                    result = sol::make_object(lua, std::ref(self.getComponent<T>()));
                }
            });

            return result;
        },
        "GetComponent", [&](Entity& self, std::string name, sol::this_state s) -> sol::object {
            sol::state_view lua(s);
            sol::object result = sol::make_object(lua, sol::nil);

            SerializableComponents::forEach([&](auto type) {
                using T = typename decltype(type)::type;
                auto typeName = getTypeName<T>();

                if (typeName == name)
                    result = sol::make_object(lua, std::ref(self.getComponent<T>()));
            });

            return result;
        },
        "RemoveComponent", [&](Entity& self, std::string name) {
            SerializableComponents::forEach([&](auto type) {
                using T = typename decltype(type)::type;
                auto typeName = getTypeName<T>();

                if (typeName == name)
                    self.removeComponent<T>();
            });
        },
        "Destroy", [&](Entity& self) {
            self.addComponent<Destroy>();
        },
        "BindScript", [&](Entity& self, std::string name) {
            bindScript(name, self);
        }
    );

    // add tables
    mLua["KeyCode"] = mLua.create_table_with(
        // --- Special & Whitespace Keys ---
        "Space",         GLFW_KEY_SPACE,
        "Apostrophe",    GLFW_KEY_APOSTROPHE,
        "Comma",         GLFW_KEY_COMMA,
        "Minus",         GLFW_KEY_MINUS,
        "Period",        GLFW_KEY_PERIOD,
        "Slash",         GLFW_KEY_SLASH,
        "Semicolon",     GLFW_KEY_SEMICOLON,
        "Equal",         GLFW_KEY_EQUAL,
        "LeftBracket",   GLFW_KEY_LEFT_BRACKET,
        "Backslash",     GLFW_KEY_BACKSLASH,
        "RightBracket",  GLFW_KEY_RIGHT_BRACKET,
        "GraveAccent",   GLFW_KEY_GRAVE_ACCENT, // ` key

        // --- Function Keys ---
        "Escape",        GLFW_KEY_ESCAPE,
        "Enter",         GLFW_KEY_ENTER,
        "Tab",           GLFW_KEY_TAB,
        "Backspace",     GLFW_KEY_BACKSPACE,
        "Insert",        GLFW_KEY_INSERT,
        "Delete",        GLFW_KEY_DELETE,
        "Right",         GLFW_KEY_RIGHT,
        "Left",          GLFW_KEY_LEFT,
        "Down",          GLFW_KEY_DOWN,
        "Up",            GLFW_KEY_UP,
        "PageUp",        GLFW_KEY_PAGE_UP,
        "PageDown",      GLFW_KEY_PAGE_DOWN,
        "Home",          GLFW_KEY_HOME,
        "End",           GLFW_KEY_END,
        "CapsLock",      GLFW_KEY_CAPS_LOCK,
        "ScrollLock",    GLFW_KEY_SCROLL_LOCK,
        "NumLock",       GLFW_KEY_NUM_LOCK,
        "PrintScreen",   GLFW_KEY_PRINT_SCREEN,
        "Pause",         GLFW_KEY_PAUSE,

        // --- F-Keys (F1 to F12) ---
        "F1",            GLFW_KEY_F1,
        "F2",            GLFW_KEY_F2,
        "F3",            GLFW_KEY_F3,
        "F4",            GLFW_KEY_F4,
        "F5",            GLFW_KEY_F5,
        "F6",            GLFW_KEY_F6,
        "F7",            GLFW_KEY_F7,
        "F8",            GLFW_KEY_F8,
        "F9",            GLFW_KEY_F9,
        "F10",           GLFW_KEY_F10,
        "F11",           GLFW_KEY_F11,
        "F12",           GLFW_KEY_F12,
        // GLFW supports up to F25 if your keyboard has them

        // --- Modifier Keys ---
        "LeftShift",     GLFW_KEY_LEFT_SHIFT,
        "LeftControl",   GLFW_KEY_LEFT_CONTROL,
        "LeftAlt",       GLFW_KEY_LEFT_ALT,
        "LeftSuper",     GLFW_KEY_LEFT_SUPER,    // Windows/Command key
        "RightShift",    GLFW_KEY_RIGHT_SHIFT,
        "RightControl",  GLFW_KEY_RIGHT_CONTROL,
        "RightAlt",      GLFW_KEY_RIGHT_ALT,
        "RightSuper",    GLFW_KEY_RIGHT_SUPER,   // Windows/Command key
        "Menu",          GLFW_KEY_MENU,

        // --- Alphanumeric Keys ---
        "0", GLFW_KEY_0, "1", GLFW_KEY_1, "2", GLFW_KEY_2, "3", GLFW_KEY_3,
        "4", GLFW_KEY_4, "5", GLFW_KEY_5, "6", GLFW_KEY_6, "7", GLFW_KEY_7,
        "8", GLFW_KEY_8, "9", GLFW_KEY_9,

        "A", GLFW_KEY_A, "B", GLFW_KEY_B, "C", GLFW_KEY_C, "D", GLFW_KEY_D,
        "E", GLFW_KEY_E, "F", GLFW_KEY_F, "G", GLFW_KEY_G, "H", GLFW_KEY_H,
        "I", GLFW_KEY_I, "J", GLFW_KEY_J, "K", GLFW_KEY_K, "L", GLFW_KEY_L,
        "M", GLFW_KEY_M, "N", GLFW_KEY_N, "O", GLFW_KEY_O, "P", GLFW_KEY_P,
        "Q", GLFW_KEY_Q, "R", GLFW_KEY_R, "S", GLFW_KEY_S, "T", GLFW_KEY_T,
        "U", GLFW_KEY_U, "V", GLFW_KEY_V, "W", GLFW_KEY_W, "X", GLFW_KEY_X,
        "Y", GLFW_KEY_Y, "Z", GLFW_KEY_Z,

        // --- Keypad Keys ---
        "KP_0",          GLFW_KEY_KP_0,
        "KP_1",          GLFW_KEY_KP_1,
        "KP_2",          GLFW_KEY_KP_2,
        "KP_3",          GLFW_KEY_KP_3,
        "KP_4",          GLFW_KEY_KP_4,
        "KP_5",          GLFW_KEY_KP_5,
        "KP_6",          GLFW_KEY_KP_6,
        "KP_7",          GLFW_KEY_KP_7,
        "KP_8",          GLFW_KEY_KP_8,
        "KP_9",          GLFW_KEY_KP_9,
        "KP_Decimal",    GLFW_KEY_KP_DECIMAL,
        "KP_Divide",     GLFW_KEY_KP_DIVIDE,
        "KP_Multiply",   GLFW_KEY_KP_MULTIPLY,
        "KP_Subtract",   GLFW_KEY_KP_SUBTRACT,
        "KP_Add",        GLFW_KEY_KP_ADD,
        "KP_Enter",      GLFW_KEY_KP_ENTER,
        "KP_Equal",      GLFW_KEY_KP_EQUAL
    );

    mLua["MouseButton"] = mLua.create_table_with(
        "Left", GLFW_MOUSE_BUTTON_LEFT,
        "Right", GLFW_MOUSE_BUTTON_RIGHT,
        "Wheel", GLFW_MOUSE_BUTTON_MIDDLE
    );

    mLua["Input"] = mLua.create_table_with(
        "GetKey", [&](int key) -> bool {
            InputState state = mInput.keyStates.at(key);
            return (state == InputState::HELD || state == InputState::PRESSED);
        },
        "GetKeyUp", [&](int key) -> bool {
            InputState state = mInput.keyStates.at(key);
            return (state == InputState::RELEASED);
        },
        "GetKeyDown", [&](int key) -> bool {
            InputState state = mInput.keyStates.at(key);
            return (state == InputState::PRESSED);
        },
        "GetMouseButton", [&](int button) -> bool {
            InputState state = mInput.mouse.buttonStates.at(button);
            return (state == InputState::HELD || state == InputState::PRESSED);
        },
        "GetMouseButtonUp", [&](int button) -> bool {
            InputState state = mInput.mouse.buttonStates.at(button);
            return (state == InputState::RELEASED);
        },
        "GetMouseButtonDown", [&](int button) -> bool {
            InputState state = mInput.mouse.buttonStates.at(button);
            return (state == InputState::PRESSED);
        },
        "GetMousePosition", [&]() -> glm::vec2 {
            return mInput.mouse.position;
        },
        "GetMouseDelta", [&]() -> glm::vec2 {
            return mInput.mouse.deltaPosition;
        },
        "GetMouseWheelDelta", [&]() -> float {
            return mInput.mouse.deltaWheel;
        }
    );

    mLua["Window"] = mLua.create_table_with(
        "IsCursorLocked", [&]() -> bool {
            return mVkEngine.getWindow().isCursorLocked();
        },
        "LockCursor", [&]() {
            mVkEngine.getWindow().lockCursor();
        },
        "UnlockCursor", [&]() {
            mVkEngine.getWindow().unlockCursor();
        }
    );

    mLua["SceneManager"] = mLua.create_table_with(
        "LoadSceneAsync", [&](std::string scene) {
            mSceneManager.loadSceneAsync(scene);
        },
        "FinishedLoadingScene", [&]() -> bool {
            return mSceneManager.finishedLoadingScene();
        },
        "SwitchScene", [&](std::string scene) {
            mSceneManager.switchScene(scene, mVkEngine.getImGuiContext());
        },
        "GetActiveSceneName", [&](std::string scene) {
            return mSceneManager.getActiveSceneName();
        },
        "GetActiveScene", [&]() {
            return std::ref(mSceneManager.getActiveScene());
        },
        "GetScene", [&](std::string scene) {
            return std::ref(mSceneManager.getActiveScene());
        }
    );

    mLua["Time"] = mLua.create_table_with(
        "deltaTime", 0.f
    );

    mLua["EffectManager"] = mLua.create_table_with(
        "EnableEffect", [&](const std::string& effect) {
            mVkEngine.getComputeEffectsManager().enableEffect(effect);
        },
        "DisableEffect", [&](const std::string& effect) {
            mVkEngine.getComputeEffectsManager().disableEffect(effect);
        },
        "SetValue", [&](const std::string& effect, const std::string& name, float value) {
            mVkEngine.getComputeEffectsManager().setValue(effect, name, value);
        }
    );

    // mIsInitialized = true;
    fmt::println("Initialized lua state");
}

ScriptManager::ScriptManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine), mSceneManager(mVkEngine.getSceneManager()) {
    initializeLuaState();
}

ScriptManager::~ScriptManager() {

}

void ScriptManager::loadScript(const std::string& name) {
    if (mLoadedScripts.contains(name)) {
        return;
    }

    fmt::println("Loading script: {}", name);
    std::filesystem::path filePath = mProjectPath + "/assets/scripts/" + name;

    // load script from file without executing it
    sol::load_result loadResult = mLua.load_file(filePath);

    if (!loadResult.valid()) {
        sol::error err = loadResult;
        fmt::println("failed to load script {}: {}", filePath.c_str(), err.what());
        return;
    }

    // convert the loaded file into bytecode
    sol::protected_function target = loadResult.get<sol::protected_function>();
    sol::bytecode targetBytecode = target.dump();

    // save the bytecode
    mLoadedScripts[name] = {targetBytecode, getSymbols(targetBytecode)};
}

void ScriptManager::loadAllScripts() {
    static const std::filesystem::path path(mProjectPath + "/assets/scripts");

    if (!std::filesystem::exists(path)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadScript(entry.path().filename());
    }
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
    env.set_function("GetUUID", &Entity::getUUID, &entity);
    env.set_function("GetParent", &Entity::getParent, &entity);
    env.set_function("GetChildren", &Entity::getChildren, &entity);
    env.set_function("HasParent", &Entity::hasParent, &entity);

    env.set_function("AddComponent", [&](std::string name, sol::this_state s) -> sol::object {
        sol::state_view lua(s);
        sol::object result = sol::make_object(lua, sol::nil);

        SerializableComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;
            auto typeName = getTypeName<T>();

            if (typeName == name) {
                entity.addComponent<T>();
                result = sol::make_object(lua, std::ref(entity.getComponent<T>()));
            }
        });

        return result;
    });

    env.set_function("GetComponent", [&](std::string name, sol::this_state s) -> sol::object {
        sol::state_view lua(s);
        sol::object result = sol::make_object(lua, sol::nil);

        SerializableComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;
            auto typeName = getTypeName<T>();

            if (typeName == name)
                result = sol::make_object(lua, std::ref(entity.getComponent<T>()));
        });

        return result;
    });

    env.set_function("RemoveComponent", [&](std::string name) {
        SerializableComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;
            auto typeName = getTypeName<T>();

            if (typeName == name)
                entity.removeComponent<T>();
        });
    });

    env.set_function("Destroy", [&]() {
        entity.addComponent<Destroy>();
    });

    env.set_function("BindScript", [&](std::string name) {
        bindScript(name, entity);
    });
}

void ScriptManager::update(float deltaTime, const Input& input) {
    mLua["Time"]["deltaTime"] = deltaTime;
    mInput = input;
}

void ScriptManager::onDestroy(entt::registry& registry) {
    auto view = registry.view<Destroy, Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        if (script.name == "" || script.name == "None" || !script.env["onDestroy"].valid()) {
            continue;
        }

        sol::protected_function_result result = script.env["onDestroy"]();

        if (!result.valid()) {
            sol::error err = result;
            fmt::println("onDestroy function failed {}: {}", metadata.uuid, err.what());
        }
    }
}

void ScriptManager::onUpdate(entt::registry& registry) {
    auto view = registry.view<Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        if (script.name == "" || script.name == "None" || !script.env["update"].valid()) {
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
        if (script.name == "" || script.name == "None" || script.initialized || !script.env["init"].valid()) {
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

void ScriptManager::onLateUpdate(entt::registry& registry) {
    auto view = registry.view<Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        if (script.name == "" || script.name == "None" || !script.env["late_update"].valid()) {
            continue;
        }

        sol::protected_function_result result = script.env["late_update"]();

        if (!result.valid()) {
            sol::error err = result;
            fmt::println("onLateUpdate function failed {}: {}", metadata.uuid, err.what());
        }
    }
}

void ScriptManager::onCollision(entt::registry& registry) {
    auto view = registry.view<SphereCollider, Transform>();

    for (auto it_a = view.begin(); it_a != view.end(); ++it_a) {
        // The inner loop starts one element AFTER the outer loop's current element
        for (auto it_b = std::next(it_a); it_b != view.end(); ++it_b) {
            entt::entity entityA = *it_a;
            entt::entity entityB = *it_b;

            auto& colliderA = registry.get<SphereCollider>(entityA);
            auto& colliderB = registry.get<SphereCollider>(entityB);

            auto& transformA = registry.get<Transform>(entityA);
            auto& transformB = registry.get<Transform>(entityB);

            if (glm::distance(transformA.position, transformB.position) > colliderA.radius + colliderB.radius) continue;

            if (registry.all_of<Script>(entityA)) {
                auto& script = registry.get<Script>(entityA);

                if (script.name != "" && script.name != "None" && script.env["on_collision"].valid()) {
                    sol::protected_function_result result = script.env["on_collision"]();

                    if (!result.valid()) {
                        sol::error err = result;
                        auto& metadata = registry.get<Metadata>(entityA);
                        fmt::println("on_collision function failed {}: {}", metadata.uuid, err.what());
                    }
                }
            }

            if (registry.all_of<Script>(entityB)) {
                auto& script = registry.get<Script>(entityB);

                if (script.name != "" && script.name != "None" && script.env["on_collision"].valid()) {
                    sol::protected_function_result result = script.env["on_collision"]();

                    if (!result.valid()) {
                        sol::error err = result;
                        auto& metadata = registry.get<Metadata>(entityB);
                        fmt::println("on_collision function failed {}: {}", metadata.uuid, err.what());
                    }
                }
            }
        }
    }
}

void ScriptManager::drawScriptAssets(ImGuiContext& context) {
    const uint32_t maxTextLength = 15;

    ImGui::PushFont(context.font12);
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    auto drawList = ImGui::GetWindowDrawList();
    auto& luaIcon = *mVkEngine.getAssetManager().getImGuiImage("lua-icon.png");

    ImVec2 p0 = ImGui::GetCursorScreenPos();

    uint32_t itemsPerRow = 9;
    uint32_t row = 0;
    uint32_t col = 0;

    ImVec2 spacing = context.spacing;
    float iconWidth = (1000.f - (itemsPerRow + 1) * spacing.x) / itemsPerRow;
    ImVec2 iconSize = ImVec2(iconWidth, iconWidth);
    float textHeight = ImGui::CalcTextSize("dummy.lua").y;

    for (auto& [k, v] : mLoadedScripts) {
        ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y));
        ImVec2 max = ImVec2(min.x + iconSize.x, min.y + iconSize.y);

        std::string text = k;
        if (text.length() > maxTextLength) {
            text = text.substr(0, maxTextLength - 3) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float textOffset = (iconSize.x - textSize.x) * 0.5f;

        static double lastClickTime = 0.f;

        ImGui::SetCursorScreenPos(min);
        std::string buttonName = "button" + k;
        if (ImGui::InvisibleButton(buttonName.c_str(), iconSize)) {
            double clickTime = ImGui::GetTime();

            if (clickTime - lastClickTime < 0.3f) {
                std::string filePath = mProjectPath + "/assets/scripts/" + k;

                if (fork() == 0) {
                    execlp("zeditor", "zeditor", "-n", filePath.c_str(), (char*)nullptr);
                    _exit(1); // If execlp fails
                }
            }

            lastClickTime = clickTime;
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SCRIPT_PAYLOAD", k.c_str(), k.size());

            ImGui::Image(luaIcon.id, iconSize);
            ImGui::Text(k.c_str());

            ImGui::EndDragDropSource();
        }

        ImGui::SetCursorScreenPos(min);

        ImGui::BeginGroup();
        ImGui::Image(luaIcon.id, iconSize);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - textHeight);
        ImGui::Text(text.c_str());
        ImGui::EndGroup();

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

    if (ImGui::ImageButton("AddScript", plusIcon.id, plusSize)) {
        std::string scriptName = getNewScriptName();

        std::ofstream outFile(mProjectPath + "/assets/scripts/" + scriptName);
        std::string baseScriptCode = R"(function init()

end

function update()

end

function late_update()

end)";

        outFile << baseScriptCode;
        outFile.close();

        loadScript(scriptName);
    }

    ImGui::PopStyleColor(4);
    ImGui::PopFont();
}

std::string ScriptManager::getNewScriptName() {
    std::string baseName = "NewScript";
    std::string name = baseName;
    int curr = 1;

    while (mLoadedScripts.contains(name + ".lua")) {
        name = baseName + std::to_string(curr++);
    }

    return name + ".lua";
}

void ScriptManager::refreshScripts() {
    mLoadedScripts.clear();
    loadAllScripts();
}
