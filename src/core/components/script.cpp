#include "script.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<Script>(const Script& component) {
    std::string type = getTypeName<Script>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
Script componentFromJson(const nlohmann::json& j) {
    Script newScript{};

    newScript.name = j["Name"];

    return newScript;
}

template<>
void drawComponent(Script& component, ImGuiContext& context) {
    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool open = ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopupContextItem("script_popup")) {
        if (ImGui::MenuItem("Delete")) {
            context.deletedComponent = true;
            context.componentName = getTypeName<Script>();
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
            component.env = {};
            component.initialized = false;
            component.symbols = {};
            component.name = "None";
        }

        ImGui::PopStyleColor(3);

        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, BUTTON_COLOR);

        // Drag-and-drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCRIPT_PAYLOAD")) {
                // Handle dropped data here
                std::string newScript((char*)payload->Data);
                component.name = newScript;
                context.changedScript = true;
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopStyleColor();

        ImGui::Unindent();

        ImGui::PopFont();
    }
}
