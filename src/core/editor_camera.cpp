#include "editor_camera.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
#include <glm/ext/quaternion_float.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include "vk_window.h"

#include <imgui.h>

EditorCamera::EditorCamera(float aspectRatio) {
    mAspectRatio = aspectRatio;
}

void EditorCamera::reset() {
    mPosition = glm::vec3(0.f);
}

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

void EditorCamera::update(float dt, float aspectRatio, const Input& input) {
    // ------------ pressed keys ----------------------
    if (input.isKeyPressed(GLFW_KEY_W)) {
        mVelocity.z = -1;
    }

    if (input.isKeyPressed(GLFW_KEY_A)) {
        mVelocity.x = -1;
    }

    if (input.isKeyPressed(GLFW_KEY_S)) {
        mVelocity.z = 1;
    }

    if (input.isKeyPressed(GLFW_KEY_D)) {
        mVelocity.x = 1;
    }

    if (input.isKeyPressed(GLFW_KEY_E)) {
        mVelocity.y = 1;
    }

    if (input.isKeyPressed(GLFW_KEY_Q)) {
        mVelocity.y = -1;
    }

    // ------------- released keys ------------------
    if (input.isKeyReleased(GLFW_KEY_W)) {
        mVelocity.z = 0;
    }

    if (input.isKeyReleased(GLFW_KEY_A)) {
        mVelocity.x = 0;
    }

    if (input.isKeyReleased(GLFW_KEY_S)) {
        mVelocity.z = 0;
    }

    if (input.isKeyReleased(GLFW_KEY_D)) {
        mVelocity.x = 0;
    }

    if (input.isKeyReleased(GLFW_KEY_E)) {
        mVelocity.y = 0;
    }

    if (input.isKeyReleased(GLFW_KEY_Q)) {
        mVelocity.y = 0;
    }

    mYaw += input.mouse.deltaPosition.x * MOUSE_MOTION_SENSITIVITY;
    mPitch -= input.mouse.deltaPosition.y * MOUSE_MOTION_SENSITIVITY;

    mPitch = glm::clamp(mPitch, MIN_PITCH, MAX_PITCH);

    mFov -= input.mouse.deltaWheel * MOUSE_WHEEL_SENSITIVITY;
    mFov = glm::clamp(mFov, MIN_FOV, MAX_FOV);

    glm::mat4 cameraRotation = getRotationMatrix();
    mPosition += glm::vec3(cameraRotation * glm::vec4(mVelocity * mSpeed * dt, 0.f));
    mAspectRatio = aspectRatio;
}

void EditorCamera::drawGui() {
    if (ImGui::TreeNode("Camera")) {
        ImGui::PushItemWidth(60);
        ImGui::InputFloat("speed", &mSpeed);

        ImGui::TreePop();
    }
}
