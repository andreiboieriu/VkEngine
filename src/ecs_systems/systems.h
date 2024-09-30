#pragma once

#include "entt.hpp"
#include "vk_types.h"
#include "ecs_components/components.h"

inline void renderSystem(RenderContext& context, entt::registry& registry) {
    auto view = registry.view<Transform, GLTF>();

    for (auto [entity, transform, gltf] : view.each()) {
        if (gltf.gltf == nullptr) {
            continue;
        }

        gltf.gltf->draw(transform.globalMatrix, context);
    }
}
