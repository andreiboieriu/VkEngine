#pragma once

#include "glm/ext/vector_float3.hpp"
#include "vk_loader.h"

#include <typeinfo>

enum class ComponentType {
    TRANSFORM,
    GLTF,
    VELOCITY,
    UNDEFINED
};

struct Gltf {
    std::shared_ptr<LoadedGLTF> gltf = nullptr;
    std::string name = "empty";
};

struct Velocity {
    glm::vec3 direction{};
    float speed = 0.f;
};

struct Transform {
    glm::vec3 position{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f};

    Transform() {}
    Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) :
    position(position), rotation(rotation), scale(scale) {}
};

template<typename T>
ComponentType getComponentType() {
    if (typeid(T) == typeid(Transform)) {
        return ComponentType::TRANSFORM;
    } else if (typeid(T) == typeid(Gltf)) {
        return ComponentType::GLTF;
    } else if (typeid(T) == typeid(Velocity)) {
        return ComponentType::VELOCITY;
    } else {
        return ComponentType::UNDEFINED;
    }
}
