#pragma once

#include "entity.h"
#include <fmt/core.h>
#include "glm/detail/qualifier.hpp"
#include "glm/ext/vector_float3.hpp"
#include "ecs_components/components.h"
#include <imgui.h>
#include "glm/gtc/type_ptr.hpp"

class CameraScript : public Script {

public:
    CameraScript(Entity& entity) : Script(entity) {}

    glm::vec3 offset;
    glm::vec3 forward;

    void onStart() override {
        mEntity.getComponent<Transform>().rotation = glm::radians(glm::vec3(0.0f, 230.0f, 0.0f));
        mEntity.getComponent<Transform>().position.y = 0.2f;

        offset = glm::vec3(0.f, 0.25f, -1.0f);
    }

    void onLateUpdate(float dt) override {
        Entity& target = getEntity("fighter_spaceship");

        Transform& targetTransform = target.getComponent<Transform>();
        forward = targetTransform.getForward();

        if (targetTransform.position.y <= -.5f) {
            return;
        }

        Transform& transform = mEntity.getComponent<Transform>();
        transform.position = glm::dot(targetTransform.position, forward) * forward + offset.z * forward;
        transform.position.y = targetTransform.position.y + offset.y;
    }

    void onDestroy() override {

    }

    void onDrawGUI() override {
        if (ImGui::Begin(getUUID().toString().c_str())) {
            ImGui::DragFloat("rotationY", &mEntity.getComponent<Transform>().rotation.y);
            ImGui::DragFloat("rotationX", &mEntity.getComponent<Transform>().rotation.x);
            ImGui::DragFloat("rotationZ", &mEntity.getComponent<Transform>().rotation.z);
            ImGui::End();
        }
    }


private:

};
