#include "transform.h"
#include "utils.h"

glm::mat4 Transform::getRotationMatrix() {
    return glm::toMat4(glm::normalize(rotation));
}

void Transform::update(glm::mat4 parentMatrix) {
    localMatrix = glm::translate(glm::mat4(1.f), position) *
                  getRotationMatrix() *
                  glm::scale(glm::mat4(1.f), scale);

    globalMatrix = parentMatrix * localMatrix;

    forward = glm::normalize(glm::rotate(glm::normalize(rotation), glm::vec3(0.f, 0.f, 1.f)));
    right = glm::normalize(glm::rotate(glm::normalize(rotation), glm::vec3(1.f, 0.f, 0.f)));
}

void Transform::translate(glm::vec3 direction, float distance) {
    position += direction * distance;
}

void Transform::rotate(glm::vec3 axis, float degrees) {
    rotation *= glm::angleAxis(glm::radians(degrees), axis);
}

Transform::Transform() {}
Transform::Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) :
                     position(position), rotation(rotation), scale(scale) {}

template<>
nlohmann::json componentToJson<Transform>(const Transform& component) {
    std::string type = getTypeName<Transform>();
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
Transform componentFromJson(const nlohmann::json& j) {
    Transform newTransform{};

    auto position = j["Position"];
    newTransform.position.x = position[0];
    newTransform.position.y = position[1];
    newTransform.position.z = position[2];

    auto rotation = j["Rotation"];
    newTransform.rotation.x = rotation[0];
    newTransform.rotation.y = rotation[1];
    newTransform.rotation.z = rotation[2];

    auto scale = j["Scale"];
    newTransform.scale.x = scale[0];
    newTransform.scale.y = scale[1];
    newTransform.scale.z = scale[2];

    return newTransform;
}
