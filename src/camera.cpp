#include "camera.h"
#include "SDL_events.h"
#include "SDL_keycode.h"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/fwd.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/matrix.hpp"
#include <glm/ext/quaternion_float.hpp>

#include <glm/gtc/matrix_transform.hpp>

glm::mat4 Camera::getViewMatrix() {
    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), mPosition);
    glm::mat4 cameraRotation = getRotationMatrix();

    return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotationMatrix() {
    glm::quat pitchRotation = glm::angleAxis(mPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yawRotation = glm::angleAxis(mYaw, glm::vec3(0.0f, -1.0f, 0.0f));

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

// TODO: improve + pass deltaTime
void Camera::processSDLEvent(SDL_Event& e) {
    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) {
            mVelocity.z = -1;
        }

        if (e.key.keysym.sym == SDLK_s) {
            mVelocity.z = 1;
        }

        if (e.key.keysym.sym == SDLK_a) {
            mVelocity.x = -1;
        }

        if (e.key.keysym.sym == SDLK_d) {
            mVelocity.x = 1;
        }

    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) {
            mVelocity.z = 0;
        }

        if (e.key.keysym.sym == SDLK_s) {
            mVelocity.z = 0;
        }

        if (e.key.keysym.sym == SDLK_a) {
            mVelocity.x = 0;
        }

        if (e.key.keysym.sym == SDLK_d) {
            mVelocity.x = 0;
        }
    }

    if (e.type == SDL_MOUSEMOTION) {
        mYaw += (float)e.motion.xrel / 200.f;
        mPitch -= (float)e.motion.yrel / 200.f;
    }
}

void Camera::update() {
    glm::mat4 cameraRotation = getRotationMatrix();
    mPosition += glm::vec3(cameraRotation * glm::vec4(mVelocity * 0.2f, 0.f));
}
