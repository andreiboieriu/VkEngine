#pragma once

#include "entt.hpp"
#include "vk_types.h"
#include "ecs_components/components.h"
#include "glm/gtc/matrix_transform.hpp"

extern entt::registry gRegistry;

inline void renderSystem(RenderContext& context) {
    auto view = gRegistry.view<Transform, Gltf>();

    for (auto [entity, transform, gltf] : view.each()) {
        if (gltf.gltf == nullptr) {
            continue;
        }

        glm::mat4 model = glm::mat4(1.f);
        model = glm::translate(model, transform.position);
        model = glm::rotate(model, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, transform.scale);

        gltf.gltf->draw(model, context);
    }
}

inline void velocitySystem(float dt) {
    auto view = gRegistry.view<Transform, Velocity>();

    for (auto [entity, transform, velocity] : view.each()) {
        transform.position += velocity.direction * velocity.speed * dt;
    }
}