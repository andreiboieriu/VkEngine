#pragma once

#include "entt.hpp"
#include <nlohmann/json.hpp>
#include "components/components.h"

class Entity {

public:
    Entity(const Metadata& meta, entt::registry& registry);
    ~Entity();

    template<typename T, typename... Args>
    void addComponent(Args&... args) {
        mRegistry.emplace<T>(mHandle, args...);
    }

    template<typename T>
    void addComponent(const T& component) {
        mRegistry.emplace<T>(mHandle, component);
    }

    template<typename T>
    void removeComponent() {
        mRegistry.erase<T>(mHandle);
    }

    template<typename T>
    T& getComponent() {
        return mRegistry.get<T>(mHandle);
    }

    template<typename T>
    bool hasComponent() {
        return mRegistry.all_of<T>(mHandle);
    }

    void setParent(Entity* parent) {
        mParent = parent;
    }

    void addChild(Entity* child);
    void removeChild(Entity* child);

    void deferredDestroy();

    void drawGUI();
    void propagateTransform(const glm::mat4& parentMatrix);

    const std::string& getUUID() {
        return mRegistry.get<Metadata>(mHandle).uuid;
    }

    entt::entity getHandle() {
        return mHandle;
    }

    nlohmann::json toJson();

    operator entt::entity() const { return mHandle; }

private:

    entt::entity mHandle;
    entt::registry& mRegistry;

    Entity* mParent = nullptr;
    std::vector<Entity*> mChildren;
};
