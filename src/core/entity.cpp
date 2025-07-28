#include "entity.h"

#include "components/components.h"
#include <imgui.h>

Entity::Entity(const Metadata& meta, entt::registry& registry) : mRegistry(registry) {
    mHandle = mRegistry.create();
    addComponent<Metadata>(meta);
    mUUID = meta.uuid;
}

Entity::~Entity() {
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

void Entity::propagateTransform(const glm::mat4& parentMatrix, glm::vec2 drawExtent) {
    glm::mat4 globalMatrix = parentMatrix;

    if (hasComponent<Transform>()) {
        Transform& transform = getComponent<Transform>();

        if (hasComponent<Sprite>()) {
            transform.update(parentMatrix, true, drawExtent);
        } else {
            transform.update(parentMatrix);
        }

        globalMatrix = transform.globalMatrix;
    }

    for (auto child : mChildren) {
        child->propagateTransform(globalMatrix, drawExtent);
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

void Entity::drawHierarchy(ImGuiContext& context) {
    bool isLeaf = mChildren.empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (mUUID == context.selectedEntity)
        flags |= ImGuiTreeNodeFlags_Selected;

    if (isLeaf)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    // Set colors
    ImVec4 headerColor = HOVERED_BUTTON_COLOR;
    headerColor.w = 0.7;

    ImVec4 headerHoveredColor = BUTTON_COLOR;
    headerHoveredColor.w = 0.7;

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);

    ImVec2 currPos = ImGui::GetCursorScreenPos();
    bool open = ImGui::TreeNodeEx(mUUID.c_str(), flags);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ENTITY_PAYLOAD", mUUID.c_str(), mUUID.size());

        ImGui::Text(mUUID.c_str());

        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
            // Handle dropped data here
            std::string movedUUID((char*)payload->Data);
            context.movedEntity = true;
            context.newParent = mUUID;
            context.entityName = movedUUID;
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsItemClicked()) {
        context.selectedEntity = mUUID;
    }

    std::string popupName = "entity_popup" + mUUID;
    if (ImGui::BeginPopupContextItem(popupName.c_str())) {
        if (ImGui::MenuItem("Delete")) {
            context.deletedEntity = true;
            context.entityName = mUUID;

            if (context.selectedEntity == mUUID) {
                context.selectedEntity = "";
            }
        }

        ImGui::EndPopup();
    }

    // Only recurse if it has children and is open
    if (!isLeaf && open) {
        for (auto child : mChildren) {
            child->drawHierarchy(context);
        }
        ImGui::TreePop();
    }

    ImGui::PopStyleColor(2);
}
