#include "camera.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
#include <glm/ext/quaternion_float.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

glm::mat4 EditorCamera::getViewMatrix() {
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), mPosition);
    glm::mat4 cameraRotation = getRotationMatrix();

    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 EditorCamera::getProjectionMatrix() {
    // invert far and near
    glm::mat4 projection = glm::perspective(glm::radians(mFov), mAspectRatio, mFar, mNear);

    // projection[1][1] *= -1.0f;

    return projection;
}

glm::mat4 EditorCamera::getRotationMatrix() {
    glm::quat pitchRotation = glm::angleAxis(glm::radians(mPitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yawRotation = glm::angleAxis(glm::radians(mYaw), glm::vec3(0.0f, -1.0f, 0.0f));

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void EditorCamera::update(float dt, float aspectRatio) {
    // if (!userInput.GuiMode) {
    //     if (userInput.pressedKeys.contains(SDLK_w)) {
    //         mVelocity.z = -1;
    //     }

    //     if (userInput.pressedKeys.contains(SDLK_a)) {
    //         mVelocity.x = -1;
    //     }

    //     if (userInput.pressedKeys.contains(SDLK_s)) {
    //         mVelocity.z = 1;
    //     }

    //     if (userInput.pressedKeys.contains(SDLK_d)) {
    //         mVelocity.x = 1;
    //     }

    //     if (userInput.releasedKeys.contains(SDLK_w)) {
    //         mVelocity.z = 0;
    //     }

    //     if (userInput.releasedKeys.contains(SDLK_a)) {
    //         mVelocity.x = 0;
    //     }

    //     if (userInput.releasedKeys.contains(SDLK_s)) {
    //         mVelocity.z = 0;
    //     }

    //     if (userInput.releasedKeys.contains(SDLK_d)) {
    //         mVelocity.x = 0;
    //     }

    //     mYaw += userInput.mouseXRel * MOUSE_MOTION_SENSITIVITY;
    //     mPitch -= userInput.mouseYRel * MOUSE_MOTION_SENSITIVITY;

    //     mPitch = glm::clamp(mPitch, MIN_PITCH, MAX_PITCH);

    //     mFov -= userInput.mouseWheel * MOUSE_WHEEL_SENSITIVITY;
    //     mFov = glm::clamp(mFov, MIN_FOV, MAX_FOV);
    // }

    // glm::mat4 cameraRotation = getRotationMatrix();
    // mPosition += glm::vec3(cameraRotation * glm::vec4(mVelocity * mSpeed * dt, 0.f));
    // mAspectRatio = aspectRatio;
}

void EditorCamera::drawGui() {
    if (ImGui::TreeNode("Camera")) {
        ImGui::PushItemWidth(60);
        ImGui::InputFloat("speed", &mSpeed);

        ImGui::TreePop();
    }
}
