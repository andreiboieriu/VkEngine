#pragma once

#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/gtx/quaternion.hpp"
#include "vk_loader.h"
#include "entt.hpp"
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#include "uuid.h"

struct GLTF {
    LoadedGLTF *gltf = nullptr;
    std::string name = "empty";
};

struct Sprite {

};

struct Transform {
    glm::vec3 position{};
    glm::quat rotation = glm::quat(glm::vec3(0.f, 0.f, 0.f));
    glm::vec3 scale{1.f};

    glm::mat4 localMatrix{1.f};
    glm::mat4 globalMatrix{1.f};

    glm::vec3 forward;
    glm::vec3 right;

    glm::mat4 getRotationMatrix() {
        return glm::toMat4(glm::normalize(rotation));
    }

    void update(glm::mat4 parentMatrix) {
        localMatrix = glm::translate(glm::mat4(1.f), position) *
                      getRotationMatrix() *
                      glm::scale(glm::mat4(1.f), scale);

        globalMatrix = parentMatrix * localMatrix;

        forward = glm::normalize(glm::rotate(glm::normalize(rotation), glm::vec3(0.f, 0.f, 1.f)));
        right = glm::normalize(glm::rotate(glm::normalize(rotation), glm::vec3(1.f, 0.f, 0.f)));
    }

    void translate(glm::vec3 direction, float distance) {
        position += direction * distance;
    }

    void rotate(glm::vec3 axis, float degrees) {
        rotation *= glm::angleAxis(glm::radians(degrees), axis);
    }

    Transform() {}
    Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) :
    position(position), rotation(rotation), scale(scale) {}
};

struct Script {
    std::string path = "";
    sol::environment env;
    std::unordered_map<std::string, sol::type> symbols;
};

struct Camera {
    float fov = 70.f;
    float aspectRatio;
    float near = 0.1f;
    float far = 1000.f;

    void updateMatrices(const Transform& transform);

    glm::mat4 viewMatrix = glm::mat4(1.f);
    glm::mat4 projectionMatrix = glm::mat4(1.f);
    glm::vec3 globalPosition = glm::vec3(0.f);
};

struct Destroy {};

struct UUIDComponent {
    UUID uuid = UUID::null();
};

struct SphereCollider {
    float radius = 0.f;
};

template<typename T>
std::string componentToString();

template<typename T>
nlohmann::json componentToJson(const T& component);

// type list for component types
template<typename... Component>
struct ComponentList{};

using AllComponents = ComponentList<GLTF, Transform, Script, Destroy, SphereCollider>;
using SerializableComponents = ComponentList<GLTF, Transform, Script, SphereCollider>;
