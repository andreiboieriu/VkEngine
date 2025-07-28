#include "sphere_collider.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<SphereCollider>(const SphereCollider& component) {
    std::string type = getTypeName<SphereCollider>();
    nlohmann::json j;

    j[type]["Radius"] = component.radius;

    return j;
}

template<>
SphereCollider componentFromJson(const nlohmann::json& j) {
    SphereCollider newSphereCollider{};

    newSphereCollider.radius = j["Radius"];

    return newSphereCollider;
}

template<>
void drawComponent(SphereCollider& component, ImGuiContext& context) {
    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool open = ImGui::CollapsingHeader("Sphere collider", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("sphere_collider_popup")) {
        if (ImGui::MenuItem("Delete")) {
            context.deletedComponent = true;
            context.componentName = getTypeName<SphereCollider>();
        }
        ImGui::EndPopup();
    }

    if (open) {
        ImGui::PushFont(context.font12);

        ImGui::Indent();
        ImGui::InputFloat("Radius", &component.radius);
        ImGui::Unindent();

        ImGui::PopFont();
    }
}
