#pragma once

#include "entt.hpp"
#include <nlohmann/json.hpp>
#include "uuid.h"
#include "vk_loader.h"

class Entity {

public:
    Entity(const UUID& uuid, entt::registry& registry);
    ~Entity();

    template<typename T, typename... Args>
    void addComponent(Args&... args) {
        mRegistry.emplace<T>(mHandle, args...);
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

    // void bindScript(const std::string& name);
    void bindGLTF(const std::string& name, std::shared_ptr<LoadedGLTF> gltf);

    void setParent(const UUID& parentUUID, bool removeFromFormerParent = true);
    void deferredDestroy();

    void drawGUI();
    void propagateTransform(const glm::mat4& parentMatrix);

    const UUID& getUUID() {
        return mUUID;
    }

    nlohmann::json toJson();

    operator entt::entity() const { return mHandle; }

private:

    void addChild(const UUID& childUUID);
    void removeChild(const UUID& childUUID);

    void restoreHierarchy();


    entt::entity mHandle;
    UUID mUUID;
    entt::registry& mRegistry;

    UUID mParentUUID = UUID::null();
    std::vector<UUID> mChildrenUUIDs;

};
