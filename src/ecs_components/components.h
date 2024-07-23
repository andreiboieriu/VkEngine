#pragma once

#include "ecs_components/gltf.h"
#include "ecs_components/transform.h"
#include <typeinfo>

enum class ComponentType {
    TRANSFORM,
    GLTF,
    UNDEFINED
};

template<typename T>
ComponentType getComponentType() {
    if (typeid(T) == typeid(Transform)) {
        return ComponentType::TRANSFORM;
    } else if (typeid(T) == typeid(GLTF)) {
        return ComponentType::GLTF;
    } else {
        return ComponentType::UNDEFINED;
    }
}
