#include "gltf.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<GLTF>(const GLTF& component) {
    std::string type = getTypeName<GLTF>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
GLTF componentFromJson(const nlohmann::json& j) {
    GLTF newGLTF{};

    newGLTF.name = j["Name"];

    return newGLTF;
}

template<>
void drawComponent(GLTF& component, ImGuiContext& context) {
    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool open = ImGui::CollapsingHeader("glTF", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("gltf_popup")) {
        if (ImGui::MenuItem("Delete")) {
            context.deletedComponent = true;
            context.componentName = getTypeName<GLTF>();
        }
        ImGui::EndPopup();
    }

    if (open) {
        ImGui::PushFont(context.font12);

        ImGui::Indent();

        ImVec2 size = ImVec2(227, 0); // width, height = 0 for auto
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);

        if (ImGui::Button(component.name.c_str(), size)) {
            // optional click behavior
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            component.gltf = nullptr;
            component.name = "None";
        }

        ImGui::PopStyleColor(3);

        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, BUTTON_COLOR);

        // Drag-and-drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("GLTF_PAYLOAD")) {
                // Handle dropped data here
                std::string newGltf((char*)payload->Data);
                component.name = newGltf;
                context.changedGLTF = true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopStyleColor();

        ImGui::Unindent();

        ImGui::PopFont();
    }
}
