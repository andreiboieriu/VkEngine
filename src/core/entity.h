#pragma once

#include "entt.hpp"
#include <nlohmann/json.hpp>
#include "components/components.h"
#include "colors.h"

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

    Entity* getParent() {
        return mParent;
    }

    void setParent(Entity* parent) {
        mParent = parent;
    }

    void addChild(Entity* child);
    void removeChild(Entity* child);

    void deferredDestroy();

    void drawHierarchy(ImGuiContext& context);

    void propagateTransform(const glm::mat4& parentMatrix, glm::vec2 drawExtent);

    const std::string& getUUID() {
        return mRegistry.get<Metadata>(mHandle).uuid;
    }

    entt::entity getHandle() {
        return mHandle;
    }

    std::vector<Entity*>& getChildren() {
        return mChildren;
    }

    bool hasParent() {
        return mParent != nullptr;
    }

    nlohmann::json toJson();

    operator entt::entity() const { return mHandle; }

private:
    std::string mUUID;
    entt::entity mHandle;
    entt::registry& mRegistry;

    Entity* mParent = nullptr;
    std::vector<Entity*> mChildren;

    bool mRenaming = false;
};
