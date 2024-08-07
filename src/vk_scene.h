#pragma once

#include "deletion_queue.h"
#include "vk_types.h"
#include "camera.h"
#include "vk_loader.h"
#include "skybox.h"

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
    void draw(RenderContext &context);

    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void drawSkybox(VkCommandBuffer commandBuffer) {
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

    SceneData mSceneData;
    
	Camera mCamera;

    std::string mName;

    std::shared_ptr<LoadedGLTF> mTestGLTF;

    VkDeviceSize mGlobalDescriptorOffset;

    std::unique_ptr<Skybox> mSkybox;
};