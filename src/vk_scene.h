#pragma once

#include "deletion_queue.h"
#include "vk_types.h"
#include "camera.h"
#include "vk_loader.h"
#include "skybox.h"
#include "scene_node.h"

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
    };
    
    Scene3D(const std::string& name);
    ~Scene3D();

    void update(float dt, float aspectRatio, const UserInput& userInput);
    void drawGui();

    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void drawSkybox(VkCommandBuffer commandBuffer) {
        if (mSkybox != nullptr)
            mSkybox->draw(commandBuffer);
    }

    const glm::mat4 getViewProj() {
        return mCamera.getProjectionMatrix() * mCamera.getViewMatrix();
    }
    
private:
    void init();
    void freeResources();

    DeletionQueue mDeletionQueue;

    DescriptorData mSceneDataDescriptor;
    AllocatedBuffer mSceneDataBuffer;
    VkDeviceSize mGlobalDescriptorOffset;

    SceneData mSceneData;
    
	Camera mCamera;

    std::string mName;
    std::unique_ptr<Skybox> mSkybox = nullptr;

    std::vector<std::shared_ptr<SceneNode>> mTopNodes;

    int mCurrNodeId = 0;
};