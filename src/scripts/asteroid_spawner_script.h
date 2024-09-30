#pragma once

#include "glm/common.hpp"
#include "script.h"
#include "ecs_components/components.h"
#include <imgui.h>
#include "glm/gtc/type_ptr.hpp"

class AsteroidSpawnerScript : public Script {

public:
    AsteroidSpawnerScript(Entity& entity) : Script(entity) {}

    float timeElapsed = 2.f;
    int nextID = 0;

    void onStart() override {

    }

    void onUpdate(float dt) override {
        Transform& spaceshipTransform = getEntity("fighter_spaceship").getComponent<Transform>();

        if (spaceshipTransform.position.y < 0.f) {
            return;
        }

        timeElapsed += dt;

        if (timeElapsed > 2.f) {
            timeElapsed -= 2.f;

            Entity& newAsteroid = createEntity("asteroid" + std::to_string(nextID++));
            newAsteroid.addComponent<Transform>();
            newAsteroid.addComponent<Scriptable>();
            newAsteroid.addComponent<GLTF>();
            newAsteroid.addComponent<SphereCollider>();

            newAsteroid.bindGLTF("asteroid");
            newAsteroid.bindScript("AsteroidScript");

            newAsteroid.setParent(getUUID());

            newAsteroid.getComponent<Transform>().position = spaceshipTransform.position + spaceshipTransform.getForward() * 20.f;
            newAsteroid.getComponent<SphereCollider>().radius = 1.f;
        }
    }

    void onDestroy() override {

    }

    void onDrawGUI() override {

    }


private:

};
