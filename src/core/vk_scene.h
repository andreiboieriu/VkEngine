#pragma once

#include "entity.h"

#include "deletion_queue.h"
#include "vk_types.h"
#include <nlohmann/json.hpp>
#include "entt.hpp"
#include "vk_window.h"
#include "script_manager.h"
#include "asset_manager.h"

// forward reference
class VulkanEngine;
class Entity;

class Scene3D {

public:
    Scene3D(const std::filesystem::path& path, VulkanEngine& VkEngine);
    ~Scene3D();

    void update(float deltaTime, const Input& input);
    void render(RenderContext& renderContext);
    void drawGui();

    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    const glm::mat4 getViewProj();
    void saveToFile();

    void loadFromFile(const std::filesystem::path& filePath);
    nlohmann::json toJson();

    Entity* createEntity(Metadata meta);
    Entity* createEntity(const nlohmann::json& metaJson);
    Entity* createEntity();

    Entity* getEntity(const std::string& name);
    void destroyEntity(const std::string& name);

    // systems
    void propagateTransform();
    void cleanupEntities();

private:
    void init();
    void freeResources();

    VulkanEngine& mVkEngine;
    AssetManager& mAssetManager;

    DeletionQueue mDeletionQueue;

    DescriptorData mSceneDataDescriptor;
    AllocatedBuffer mSceneDataBuffer;
    VkDeviceSize mGlobalDescriptorOffset;

    SceneData mSceneData;

    std::string mName;
    SkyboxAsset *mSkybox = nullptr;
    std::unique_ptr<ScriptManager> mScriptManager = nullptr;

    entt::registry mRegistry;

    std::vector<Entity*> mRootEntities; // has to be declared before entity map
    std::unordered_map<std::string, std::unique_ptr<Entity>> mEntities;
    Entity *mCameraEntity = nullptr;

    uint64_t mNextEntityID = 1;

    friend class Entity;
    friend class Script;
};
