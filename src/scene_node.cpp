#include "scene_node.h"
#include "ecs_components/components.h"
#include "fmt/core.h"
#include "vk_engine.h"

#include "imgui.h"

SceneNode::SceneNode(std::string_view name) : mName(name) {
    init();
}

SceneNode::~SceneNode() {
    gRegistry.destroy(mEntity);
}

void SceneNode::init() {
    mEntity = gRegistry.create();
}

void SceneNode::drawGui() {
    if (ImGui::TreeNode(mName.c_str())) {
        // for (auto& node : mChildren) {
        //     node.drawGui();
        // }

        for (auto& component : mComponentTypes) {
            switch (component) {
            case ComponentType::GLTF:
                if (ImGui::TreeNode("GLTF")) {
                    auto& gltfComponent = gRegistry.get<Gltf>(mEntity);


                    if (ImGui::BeginPopupContextItem("select gltf popup")) {
                        if (ImGui::Selectable("Hull spaceship")) {
                            gltfComponent.name = "hull_spaceship";
                            gltfComponent.gltf = VulkanEngine::get().getResourceManager().getGltf("hull_spaceship");
                        }

                        if (ImGui::Selectable("Fighter spaceship")) {
                            gltfComponent.name = "fighter_spaceship";
                            gltfComponent.gltf = VulkanEngine::get().getResourceManager().getGltf("fighter_spaceship");
                        }

                        ImGui::EndPopup();
                    }

                    if (ImGui::Button(gltfComponent.name.c_str())) {
                        ImGui::OpenPopup("select gltf popup");
                    }

                    ImGui::TreePop();
                }

                break;

            case ComponentType::TRANSFORM:
                if (ImGui::TreeNode("Transform")) {
                    auto& transformComponent = gRegistry.get<Transform>(mEntity);

                    ImGui::Text("position");
                    ImGui::PushItemWidth(100);
                    ImGui::SameLine(); ImGui::InputFloat("px", &transformComponent.position.x, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("py", &transformComponent.position.y, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("pz", &transformComponent.position.z, 1.0f);
            
                    ImGui::Text("rotation");
                    ImGui::PushItemWidth(100);
                    ImGui::SameLine(); ImGui::InputFloat("rx", &transformComponent.rotation.x, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("ry", &transformComponent.rotation.y, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("rz", &transformComponent.rotation.z, 1.0f);
            
                    ImGui::Text("scale");
                    ImGui::PushItemWidth(100);
                    ImGui::SameLine(); ImGui::InputFloat("sx", &transformComponent.scale.x, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("sy", &transformComponent.scale.y, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("sz", &transformComponent.scale.z, 1.0f);
            
                    ImGui::TreePop();
                }

                break;

            case ComponentType::VELOCITY:
                if (ImGui::TreeNode("Velocity")) {
                    auto& velocityComponent = gRegistry.get<Velocity>(mEntity);

                    ImGui::Text("direction");
                    ImGui::PushItemWidth(100);
                    ImGui::SameLine(); ImGui::InputFloat("px", &velocityComponent.direction.x, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("py", &velocityComponent.direction.y, 1.0f);
                    ImGui::SameLine(); ImGui::InputFloat("pz", &velocityComponent.direction.z, 1.0f);

                    ImGui::InputFloat("speed", &velocityComponent.speed);


                    ImGui::TreePop();
                }

                break;

            default:
                break;
            }
        }

        if (ImGui::BeginPopupContextItem("add component popup")) {
            if (!mComponentTypes.contains(ComponentType::TRANSFORM) && ImGui::Selectable("Transform")) {
                addComponent<Transform>();
                mComponentTypes.insert(ComponentType::TRANSFORM);
            }

            if (!mComponentTypes.contains(ComponentType::GLTF) && ImGui::Selectable("GLTF")) {
                addComponent<Gltf>();
                mComponentTypes.insert(ComponentType::GLTF);
            }

            if (!mComponentTypes.contains(ComponentType::VELOCITY)&& ImGui::Selectable("Velocity")) {
                addComponent<Velocity>();
                mComponentTypes.insert(ComponentType::VELOCITY);
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button("Add component")) {
            ImGui::OpenPopup("add component popup");
        }

        ImGui::TreePop();
    }
}