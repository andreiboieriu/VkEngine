#pragma once

#include "entity.h"

#include "deletion_queue.h"
#include "vk_types.h"
#include <nlohmann/json.hpp>
#include "entt.hpp"
#include "vk_window.h"
#include "script_manager.h"
#include "asset_manager.h"
#include "colors.h"
#include "editor_camera.h"

// forward reference
class VulkanEngine;
class Entity;

class Scene3D {

public:
    Scene3D(std::filesystem::path path, VulkanEngine& VkEngine);
    Scene3D(VulkanEngine& vkEngine);

    ~Scene3D();

    void update(float deltaTime, const Input& input);
    void render(RenderContext& renderContext);
    void drawHierarchy(ImGuiContext& context);
    void drawConfig(ImGuiContext& context);
    void drawInspector(ImGuiContext& context);

    void refreshScripts();

    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int frameNumber);

    const glm::mat4 getViewProj();
    void saveToFile(const std::filesystem::path& filePath);

    void loadFromFile(std::filesystem::path filePath);
    nlohmann::json toJson();

    Entity* createEntity(Metadata meta);
    Entity* createEntity(const nlohmann::json& metaJson);
    Entity* createEntity(const std::string& name);

    Entity* getEntity(const std::string& name);
    void deleteEntity(const std::string& name);
    void moveEntity(const std::string& name, const std::string& parent);

    // systems
    void propagateTransform();
    void cleanupEntities();
    void initSceneDataBuffers();

    void editorModeUpdate(EditorCamera& camera);
    void updateUBO(int frameNumber);

    SceneData& getSceneData() {
        return mSceneData;
    }

    glm::vec4 getSunScreenPos() {
        return mSunScreenPos;
    }

private:
    void init();
    void computeSunScreenPos();
    void freeResources();

    VulkanEngine& mVkEngine;
    AssetManager& mAssetManager;
    ScriptManager& mScriptManager;

    DeletionQueue mDeletionQueue;

    struct SceneDataUBO {
        AllocatedBuffer buffer;
        VkDeviceSize globalDescriptorOffset;
    };

    std::array<SceneDataUBO, 3> mSceneDataUBOs;

    SceneData mSceneData;

    SkyboxAsset *mSkybox = nullptr;

    entt::registry mRegistry;

    std::vector<Entity*> mRootEntities; // has to be declared before entity map
    std::unordered_map<std::string, std::unique_ptr<Entity>> mEntities;
    Entity *mCameraEntity = nullptr;

    friend class Entity;
    friend class Script;

    float mCameraFov;
    float mNear;
    float mFar;
    glm::vec4 mSunScreenPos; // x = x coord, y = y coord, z = 0.0 if not in view, else 1.0
    bool mAddingEntity = false;
};
