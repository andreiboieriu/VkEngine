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

class GameObject {

public:
    GameObject();
    ~GameObject();

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

    // store component names here for easy UI access
    std::set<ComponentType> mComponentTypes;

    std::vector<std::shared_ptr<GameObject>> mChildren;
    entt::entity mEntity;
};

// example
// transform component

// UI -> reference to components
// -> component types