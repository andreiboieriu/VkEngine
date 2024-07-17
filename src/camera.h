#include "SDL_events.h"
#include "vk_types.h"

class Camera {
public:

    glm::mat4 getViewMatrix();
    glm::mat4 getRotationMatrix();

    void processSDLEvent(SDL_Event& e);
    void update();

private:

    glm::vec3 mVelocity{};
    glm::vec3 mPosition{};

    // vertical rotation
    float mPitch = 0.f;

    // horizontal rotation
    float mYaw = 0.f;
};
