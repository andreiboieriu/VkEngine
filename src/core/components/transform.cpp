#include "transform.h"
#include "utils.h"

glm::mat4 Transform::getRotationMatrix() {
    return glm::toMat4(rotation);
}

void Transform::update(glm::mat4 parentMatrix, bool isSprite, glm::vec2 drawExtent) {
    if (isSprite) {
        localMatrix = glm::translate(glm::mat4(1.f), position * glm::vec3(drawExtent, 1.f)) *
                        getRotationMatrix() *
                        glm::scale(glm::mat4(1.f), scale * glm::vec3(drawExtent, 1.f));
    } else {
        localMatrix = glm::translate(glm::mat4(1.f), position) *
                        getRotationMatrix() *
                        glm::scale(glm::mat4(1.f), scale);
    }

    globalMatrix = parentMatrix * localMatrix;

    forward = glm::rotate(rotation, glm::vec3(0.f, 0.f, 1.f));
    right = glm::rotate(rotation, glm::vec3(1.f, 0.f, 0.f));
}

void Transform::translate(glm::vec3 direction, float distance) {
    position += direction * distance;
}

void Transform::rotate(glm::vec3 axis, float degrees) {
    rotation *= glm::angleAxis(glm::radians(degrees), axis);
    rotation = glm::normalize(rotation);

    forward = glm::rotate(rotation, glm::vec3(0.f, 0.f, 1.f));
    right = glm::rotate(rotation, glm::vec3(1.f, 0.f, 0.f));
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
        component.eulerRotation.x,
        component.eulerRotation.y,
        component.eulerRotation.z
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
    newTransform.eulerRotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
    newTransform.rotation = glm::quat(glm::radians(newTransform.eulerRotation));

    auto scale = j["Scale"];
    newTransform.scale.x = scale[0];
    newTransform.scale.y = scale[1];
    newTransform.scale.z = scale[2];

    return newTransform;
}

template<>
void drawComponent(Transform& component, ImGuiContext& context) {
    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool open = ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("transform_popup")) {
        if (ImGui::MenuItem("Delete")) {
            context.deletedComponent = true;
            context.componentName = getTypeName<Transform>();
        }
        ImGui::EndPopup();
    }

    if (open) {
        ImGui::PushFont(context.font12);

        ImGui::Indent();
        ImGui::DragFloat3("Position", &component.position.x);

        if (ImGui::DragFloat3("Rotation", &component.eulerRotation.x)) {
            component.rotation = glm::quat(glm::radians(component.eulerRotation));
        }

        ImGui::DragFloat3("Scale", &component.scale.x);
        ImGui::Unindent();

        ImGui::PopFont();
    }
}
