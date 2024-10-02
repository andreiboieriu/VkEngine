#pragma once

#include "uuid.h"
#include "entity.h"

#include "deletion_queue.h"
#include "vk_types.h"
#include "skybox.h"
#include <nlohmann/json.hpp>
#include "entt.hpp"
#include "vk_window.h"
#include "script_manager.h"

class Entity;
class UUID;

class Scene3D {

public:

    struct SceneData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewProjection;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection;
        glm::vec4 sunlightColor;
        glm::vec4 viewPosition;
        glm::vec4 data;
    };

    Scene3D(const std::string& name);
    ~Scene3D();

    void update();
    void render(RenderContext& renderContext);
    void drawGui();

    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void drawSkybox(VkCommandBuffer commandBuffer) {
        if (mSkybox != nullptr)
            mSkybox->draw(commandBuffer);
    }

    const glm::mat4 getViewProj();
    void saveToFile();

    void loadFromFile(std::filesystem::path filePath);
    nlohmann::json toJson();

    Entity& createEntity(const UUID& name = UUID());
    Entity& getEntity(const UUID& name);
    void destroyEntity(const UUID& name);

    // systems
    void propagateTransform();
    void cleanupEntities();

private:
    void init();
    void freeResources();

    DeletionQueue mDeletionQueue;

    DescriptorData mSceneDataDescriptor;
    AllocatedBuffer mSceneDataBuffer;
    VkDeviceSize mGlobalDescriptorOffset;

    SceneData mSceneData;

    std::string mName;
    std::unique_ptr<Skybox> mSkybox = nullptr;
    std::unique_ptr<ScriptManager> mScriptManager = nullptr;

    entt::registry mRegistry;

    std::vector<UUID> mRootEntityUUIDs; // has to be declared before entity map
    std::unordered_map<UUID, std::unique_ptr<Entity>> mEntityMap;
    UUID mMainCameraHolder = UUID::null();

    friend class Entity;
    friend class Script;
};
