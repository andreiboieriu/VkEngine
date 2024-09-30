#pragma once

#include "glm/common.hpp"
#include "ecs_components/components.h"
#include <imgui.h>
#include "glm/gtc/type_ptr.hpp"

class SpaceshipScript : public Script {

public:
    SpaceshipScript(Entity& entity) : Script(entity) {}

    float maxRotation = 60.f;
    float maxOffset = .7f;

    float rotationSpeed = 130.f;
    glm::vec3 moveSpeed = glm::vec3(1.6f, 0.0f, 1.f);

    float currOffset = 0.f;
    float currRotation = 0.f;
    float timeElapsed = 0.f;

    glm::vec3 initialRotation = glm::radians(glm::vec3(0.f, 50.f, 0.f));

    glm::vec3 initialRight;
    bool start = false;
    bool pressedSpace = false;
    float targetRotation;

    void onStart() override {
        Transform& transform = mEntity.getComponent<Transform>();
        // transform.scale = glm::vec3(1.2f);
        transform.position.y = -0.5f;
        transform.rotation = glm::radians(glm::vec3(0.f, 50.f, 0.f));

        transform.position -= transform.getForward() * 0.2f;
        initialRight = transform.getRight();

        mEntity.getComponent<SphereCollider>().radius = 1.f;

        // init camera
        getEntity("main_camera").getComponent<Transform>().position = transform.position - transform.getForward() + glm::vec3(0.0f, 0.25f, 0.0f);
    }

    void rotateAroundAxis(glm::vec3 axis, float degrees) {
        glm::quat& rotation = mEntity.getComponent<Transform>().rotation;

        rotation = glm::angleAxis(glm::radians(degrees), axis) * rotation;
        rotation = glm::normalize(rotation);
    }

    void onUpdate(float dt) override {
        const UserInput& userInput = getUserInput();

        if (!start) {
            // rotateAroundAxis(glm::vec3(0.0f, 1.0f, 0.0f), 45.f * dt);
            glm::quat& rotation = mEntity.getComponent<Transform>().rotation;

            if (userInput.keyboardState[SDL_SCANCODE_SPACE]) {
                pressedSpace = true;
                targetRotation = std::ceil(rotation.y / 360.f) * 360.f + 50.f;
                start = true;
            }


            if (pressedSpace && rotation.y >= targetRotation) {
                rotation = glm::radians(glm::vec3(0.f, 50.f, 0.f));
                start = true;
            }

            return;
        }

        Transform& transform = mEntity.getComponent<Transform>();
        transform.position += transform.getForward() * moveSpeed.z * dt;
        transform.position.y += 0.2f * dt;

        if (transform.position.y >= 0.f) {
            transform.position.y = 0.f;
        } else {
            return;
        }

        timeElapsed += dt;

        if (timeElapsed > 1.f) {
            moveSpeed.z *= 1.01f;
            timeElapsed -= 1.f;

            fmt::println("curr speed: {}", moveSpeed.z);
        }

        bool moved = false;

        if (userInput.keyboardState[SDL_SCANCODE_A]) {
            currRotation -= rotationSpeed * dt;
            currOffset += moveSpeed.x * dt;

            // rotateAroundAxis(transform.getForward(), -rotationSpeed * dt);
            // transform.position += transform.getRight() * moveSpeed * dt;

            moved = true;
        }

        if (userInput.keyboardState[SDL_SCANCODE_D]) {
            currRotation += rotationSpeed * dt;
            currOffset -= moveSpeed.x * dt;

            // rotateAroundAxis(transform.getForward(), rotationSpeed * dt);
            // transform.position -= transform.getRight() * moveSpeed * dt;

            moved = true;
        }


        if (!moved && currRotation != 0.f) {
            if (currRotation > 0.f) {
                currRotation -= rotationSpeed * dt;
                currRotation = glm::clamp(currRotation, 0.f, maxRotation);
            } else {
                currRotation += rotationSpeed * dt;
                currRotation = glm::clamp(currRotation, -maxRotation, 0.f);
            }
        }

        float y = transform.position.y;

        currOffset = glm::clamp(currOffset, -maxOffset, maxOffset);
        transform.position = glm::dot(transform.position, transform.getForward()) * transform.getForward() + initialRight * currOffset;

        transform.position.y = y;

        currRotation = glm::clamp(currRotation, -maxRotation, maxRotation);
        transform.rotation = glm::angleAxis(glm::radians(currRotation), transform.getForward()) * glm::quat(initialRotation);

    }

    void onDestroy() override {

    }

    void onDrawGUI() override {
        if (ImGui::Begin(getUUID().toString().c_str())) {
            // draw gui for transform component
            Transform& transform = mEntity.getComponent<Transform>();
            ImGui::DragFloat3("Position", glm::value_ptr(transform.position), 0.1f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(transform.rotation), 0.1f);
            ImGui::DragFloat3("Scale", glm::value_ptr(transform.scale), 0.1f);
            ImGui::End();
        }
    }


private:

};
