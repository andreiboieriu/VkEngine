#include "entity.h"

#include "ecs_components/components.h"
#include "glm/ext/matrix_transform.hpp"
#include <imgui.h>
#include "vk_engine.h"

UUID::UUID() {
    mUUID = defaultBaseID + std::to_string(nextID++);

    fmt::println("UUID constructor called, nextID = {}", nextID);
}

UUID::UUID(const std::string& name) {
    mUUID = name;
}

const UUID UUID::null() {
    return UUID(nullID);
}

Entity::Entity(const UUID& uuid, entt::registry& registry) : mUUID(uuid), mRegistry(registry) {
    mHandle = mRegistry.create();

    addComponent<UUIDComponent>();
    getComponent<UUIDComponent>().uuid = uuid;
}

Entity::~Entity() {
    if (hasComponent<Script>()) {
        // getComponent<Script>().script->onDestroy();
    }

    restoreHierarchy();

    mRegistry.destroy(mHandle);
}

// void Entity::bindScript(const std::string& name) {
//     if (!hasComponent<Script>()) {
//         fmt::println("Bind script error in entity named {}: No scriptable component found", mUUID.toString());
//         return;
//     }

//     getComponent<Scriptable>().script = Script::createInstance(name, *this);
//     getComponent<Scriptable>().name = name;
// }

void Entity::bindGLTF(const std::string& name) {
    if (!hasComponent<GLTF>()) {
        fmt::println("Bind GLTF error in entity named {}: No GLTF component found", mUUID.toString());
        return;
    }

    getComponent<GLTF>().gltf = VulkanEngine::get().getResourceManager().getGltf(name);
    getComponent<GLTF>().name = name;
}

void Entity::addChild(const UUID& childUUID) {
    mChildrenUUIDs.push_back(childUUID);
}

void Entity::removeChild(const UUID& childUUID) {
    mChildrenUUIDs.erase(std::remove(mChildrenUUIDs.begin(), mChildrenUUIDs.end(), childUUID), mChildrenUUIDs.end());
}

void Entity::setParent(const UUID& parentUUID, bool removeFromFormerParent) {
    // if (removeFromFormerParent && mParentUUID != UUID::null()) {
    //     // remove entity from parent's children list
    //     mScene.getEntity(mParentUUID).removeChild(mUUID);
    // }

    // mParentUUID = parentUUID;

    // // add entity to parent's children list
    // if (parentUUID != UUID::null()) {
    //     mScene.getEntity(parentUUID).addChild(mUUID);
    // }
}

void Entity::restoreHierarchy() {
    // for (auto& childUUID : mChildrenUUIDs) {
    //     Entity& childEntity = mScene.getEntity(childUUID);

    //     // update child parent
    //     childEntity.setParent(mParentUUID, false);
    // }

    // if (mParentUUID != UUID::null()) {
    //     // remove entity from parent's children list
    //     mScene.getEntity(mParentUUID).removeChild(mUUID);
    // } else {
    //     // remove entity UUID from root entities list
    //     mScene.mRootEntityUUIDs.erase(std::remove(mScene.mRootEntityUUIDs.begin(), mScene.mRootEntityUUIDs.end(), mUUID), mScene.mRootEntityUUIDs.end());

    //     // add children to root entities list
    //     mScene.mRootEntityUUIDs.insert(mScene.mRootEntityUUIDs.end(), mChildrenUUIDs.begin(), mChildrenUUIDs.end());
    // }
}

void Entity::deferredDestroy() {
    addComponent<Destroy>();
}

void Entity::propagateTransform(const glm::mat4& parentMatrix) {
    glm::mat4 globalMatrix = parentMatrix;

    if (hasComponent<Transform>()) {
        Transform& transform = getComponent<Transform>();

        // transform.localMatrix = glm::mat4(1.f);
        transform.localMatrix = glm::translate(glm::mat4(1.f), transform.position) *
                                transform.getRotationMatrix() *
                                glm::scale(glm::mat4(1.f), transform.scale);

        // apply parent matrix
        transform.globalMatrix = parentMatrix * transform.localMatrix;
        globalMatrix = transform.globalMatrix;
    }

    // for (auto& childUUID : mChildrenUUIDs) {
    //     mScene.getEntity(childUUID).propagateTransform(globalMatrix);
    // }
}

template<typename... Component>
nlohmann::json toJsonComponents(Entity& entity) {
    nlohmann::json componentsJson;

    // iterate through component list
    ([&]() {
        if (!entity.hasComponent<Component>()) {
            return;
        }

        componentsJson.push_back(componentToJson(entity.getComponent<Component>()));
    }(), ...);

    return componentsJson;
}

template<typename... Component>
nlohmann::json toJsonComponents(ComponentList<Component...>, Entity& entity) {
    return toJsonComponents<Component...>(entity);
}

nlohmann::json Entity::toJson() {
    nlohmann::json j;

    j["UUID"] = mUUID;
    j["ParentUUID"] = mParentUUID;
    j["ChildrenUUIDs"] = mChildrenUUIDs;

    j["Components"] = toJsonComponents(SerializableComponents{}, *this);

    return j;
}

void Entity::drawGUI() {
    // bool isOpen = ImGui::TreeNode(mUUID.toString().c_str());

    // if (ImGui::BeginPopupContextItem((mUUID.toString() + "popup").c_str())) {
    //     if (ImGui::Selectable("Add new entity")) {
    //         mScene.createEntity().setParent(mUUID);
    //     }

    //     if (ImGui::Selectable("Delete entity")) {
    //         deferredDestroy();
    //     }

    //     ImGui::EndPopup();
    // }

    // if (isOpen) {
    //     for (auto& childUUID : mChildrenUUIDs) {
    //         mScene.getEntity(childUUID).drawGUI();
    //     }

    //     // if (ImGui::Button("Add new entity")) {
    //     //     Entity& newEntity = createEntity();
    //     //     mRootEntityUUIDs.push_back(newEntity.mUUID);
    //     // }
    //     // ImGui::OpenPopupOnItemClick("entityPopup", ImGuiPopupFlags_MouseButtonRight);

    //     ImGui::TreePop();
    // }


}
