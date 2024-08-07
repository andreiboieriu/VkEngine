#pragma once

#include "ecs_components/components.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <memory>
#include <set>

#include "entt.hpp"

extern entt::registry gRegistry;

class SceneNode {

public:
    SceneNode(std::string_view name);
    ~SceneNode();

    template<typename T, typename... Args>
    void addComponent(Args ...args) {
        gRegistry.emplace<T>(mEntity, args...);
    }

    template<typename T>
    void removeComponent() {
        gRegistry.erase<T>(mEntity);
    }

    template<typename T>
    T& getComponent() {
        return gRegistry.get<T>(mEntity);
    }

private:
    void init();

    std::string mName;

    // store component names here for easy UI access
    std::set<ComponentType> mComponentTypes;

    std::vector<std::shared_ptr<SceneNode>> mChildren;
    entt::entity mEntity;
};
