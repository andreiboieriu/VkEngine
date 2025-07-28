#pragma once

#include "vk_types.h"
#include "vk_window.h"

class EditorCamera {
public:
    EditorCamera(float aspectRatio);

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();
    glm::mat4 getProjectionMatrix();

    glm::vec3 getPosition() {
        return mPosition;
    }

    void update(float dt, float aspectRatio, const Input& input);

    void drawGui();
    void reset();
    void resetVelocity() {
        mVelocity = glm::vec3(0.f);
    }

    float mFov = 70.f;
    float mAspectRatio;
    float mNear = 0.1f;
    float mFar = 250.f;

private:
    glm::vec3 mVelocity{};
    glm::vec3 mPosition{};

    float mSpeed = 5.f;

    const float MOUSE_MOTION_SENSITIVITY = 0.2f;
    const float MOUSE_WHEEL_SENSITIVITY = 1.0f;

    const float MAX_FOV = 170.f;
    const float MIN_FOV = 10.f;

    const float MAX_PITCH = 90.f;
    const float MIN_PITCH = -90.f;

    // vertical rotation
    float mPitch = 0.f;

    // horizontal rotation
    float mYaw = 0.f;
};
