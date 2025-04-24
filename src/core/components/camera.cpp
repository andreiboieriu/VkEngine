#include "camera.h"
#include "transform.h"
#include "utils.h"

#include <nlohmann/json.hpp>
#include <glm/gtx/matrix_decompose.hpp>

Camera::Camera() {

}

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

template<>
Camera componentFromJson(const nlohmann::json& j) {
    Camera newCamera{};

    newCamera.fov = j["Fov"];
    newCamera.near = j["Near"];
    newCamera.far = j["Far"];
    newCamera.enabled = j["Enabled"];

    return newCamera;
}

template<>
nlohmann::json componentToJson(const Camera& component) {
    std::string type = getTypeName<Camera>();
    nlohmann::json j;

    j[type]["Fov"] = component.fov;
    j[type]["Near"] = component.near;
    j[type]["Far"] = component.far;
    j[type]["Enabled"] = component.enabled;

    return j;
}
