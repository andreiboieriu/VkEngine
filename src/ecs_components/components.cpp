#include "components.h"

#include <fmt/core.h>
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/quaternion.hpp"

void Camera::updateMatrices(const Transform& transform) {
    // get rotation and translation from global transform matrix
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 skew;
    glm::vec4 perspective;

    glm::decompose(transform.globalMatrix, scale, rotation, globalPosition, skew, perspective);

    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), globalPosition);
    glm::mat4 cameraRotation = glm::toMat4(rotation);

    viewMatrix = glm::inverse(cameraTranslation * cameraRotation);
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, far, near);
}

template<typename T>
std::string componentToString() {
    if (typeid(T) == typeid(Transform)) {
        return "Transform";
    } else if (typeid(T) == typeid(GLTF)) {
        return "GLTF";
    } else {
        return "Undefined";
    }
}

template<typename T>
nlohmann::json componentToJson(const T& component) {
    nlohmann::json j(componentToString<T>());
    return j;
}

template<>
nlohmann::json componentToJson<Transform>(const Transform& component) {
    std::string type = componentToString<Transform>();
    nlohmann::json j;

    j[type]["Position"] = std::vector<float> {
        component.position.x,
        component.position.y,
        component.position.z
    };

    j[type]["Rotation"] = std::vector<float> {
        component.rotation.x,
        component.rotation.y,
        component.rotation.z
    };

    j[type]["Scale"] = std::vector<float> {
        component.scale.x,
        component.scale.y,
        component.scale.z
    };

    return j;
}

template<>
nlohmann::json componentToJson<GLTF>(const GLTF& component) {
    std::string type = componentToString<GLTF>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
nlohmann::json componentToJson<Script>(const Script& component) {
    std::string type = componentToString<Script>();
    nlohmann::json j;

    j[type]["Name"] = component.path;

    return j;
}

template<>
nlohmann::json componentToJson<SphereCollider>(const SphereCollider& component) {
    std::string type = componentToString<SphereCollider>();
    nlohmann::json j;

    j[type]["radius"] = component.radius;

    return j;
}
