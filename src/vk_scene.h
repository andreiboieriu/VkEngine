#pragma once

#include "deletion_queue.h"
#include "vk_types.h"
#include "camera.h"
#include "vk_loader.h"

class Scene3D {

public:

    struct SceneData {
        glm::mat4 view;
        glm::mat4 projection;
        glm::mat4 viewProjection;
        glm::vec4 ambientColor;
        glm::vec4 sunlightDirection;
        glm::vec4 sunlightColor;
    };
    
    Scene3D(const std::string& name, GLTFMetallicRoughness& metalRoughMaterial);
    ~Scene3D();

    void init();
    void update(float dt, float aspectRatio, const UserInput& userInput);
    void draw(RenderContext &context);

    void bindDescriptorBuffers(VkCommandBuffer commandBuffer);
    void setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);

    void freeResources();

    glm::mat4& getViewProj() {
        return mSceneData.viewProjection;
    }
    
private:

    DeletionQueue mDeletionQueue;

    DescriptorData mSceneDataDescriptor;
    AllocatedBuffer mSceneDataBuffer;

    SceneData mSceneData;
    
	Camera mCamera;

    std::string mName;

    std::shared_ptr<LoadedGLTF> mTestGLTF;

    GLTFMetallicRoughness& mMetalRoughMaterial;
    std::unique_ptr<DescriptorManager> mDescriptorManager;

    VkDeviceSize mGlobalDescriptorOffset;
};