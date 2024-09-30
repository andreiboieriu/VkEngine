#pragma once

#include "glm/common.hpp"
#include "script.h"
#include "ecs_components/components.h"
#include <imgui.h>
#include "glm/gtc/type_ptr.hpp"

class AsteroidScript : public Script {

public:
    AsteroidScript(Entity& entity) : Script(entity) {}


    void onStart() override {
        Transform& transform = mEntity.getComponent<Transform>();
        // transform.scale = glm::vec3(0.2f);
    }

    void onUpdate(float dt) override {
        Transform& transform = mEntity.getComponent<Transform>();
        transform.rotation = glm::angleAxis(glm::radians(45.f * dt), glm::normalize(glm::vec3(0.2f, 0.7f, 0.1f))) * transform.rotation;

        Transform& spaceshipTransform = getEntity("fighter_spaceship").getComponent<Transform>();
        // if (transform.position)
    }

    void onDestroy() override {

    }

    void onDrawGUI() override {

    }


private:

};
