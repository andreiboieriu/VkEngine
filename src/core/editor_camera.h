#include "vk_types.h"
#include "vk_window.h"

class EditorCamera {
public:

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();
    glm::mat4 getProjectionMatrix();

    glm::vec3 getPosition() {
        return mPosition;
    }

    void update(float dt, float aspectRatio);

    void drawGui();

private:
    glm::vec3 mVelocity{};
    glm::vec3 mPosition{};

    float mFov = 70.f;
    float mAspectRatio;
    float mNear = 0.1f;
    float mFar = 1000.f;

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
