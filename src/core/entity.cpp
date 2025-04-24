#include "entity.h"

#include "components/components.h"
#include <imgui.h>

Entity::Entity(const Metadata& meta, entt::registry& registry) : mRegistry(registry) {
    mHandle = mRegistry.create();
    addComponent<Metadata>(meta);
}

Entity::~Entity() {
    if (hasComponent<Script>()) {
        // getComponent<Script>().script->onDestroy();
    }

    mRegistry.destroy(mHandle);
}

void Entity::addChild(Entity* child) {
    mChildren.push_back(child);
}

void Entity::removeChild(Entity* child) {
    mChildren.erase(std::remove(mChildren.begin(), mChildren.end(), child), mChildren.end());
}

void Entity::deferredDestroy() {
    addComponent<Destroy>();
}

void Entity::propagateTransform(const glm::mat4& parentMatrix) {
    glm::mat4 globalMatrix = parentMatrix;

    if (hasComponent<Transform>()) {
        Transform& transform = getComponent<Transform>();

        transform.update(parentMatrix);
        globalMatrix = transform.globalMatrix;
    }

    for (auto child : mChildren) {
        child->propagateTransform(globalMatrix);
    }
}

template<typename... Component>
nlohmann::json toJsonComponents(Entity& entity) {
    nlohmann::json componentsJson;

    // iterate through component list
    ([&]() {
        if (!entity.hasComponent<Component>()) {
            return;
        }

        std::string typeName = getTypeName<Component>();

        componentsJson[typeName] = componentToJson(entity.getComponent<Component>())[typeName];
    }(), ...);

    return componentsJson;
}

template<typename... Component>
nlohmann::json toJsonComponents(ComponentList<Component...>, Entity& entity) {
    return toJsonComponents<Component...>(entity);
}

nlohmann::json Entity::toJson() {
    nlohmann::json j;

    j["ParentUUID"] = mParent != nullptr ? mParent->getUUID() : "";

    std::vector<std::string> children(mChildren.size());
    int idx = 0;

    for (auto& child : mChildren) {
        children[idx++] = child->getUUID();
    }

    j["ChildrenUUIDs"] = children;
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
