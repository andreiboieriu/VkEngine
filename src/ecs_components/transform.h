#pragma once

#include "glm/ext/vector_float3.hpp"

struct Transform {
    glm::vec3 position{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f};

    Transform() {}
    Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) :
    position(position), rotation(rotation), scale(scale) {}
};
