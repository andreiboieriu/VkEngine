#pragma once

#include "vk_types.h"
#include "vk_loader.h"

// TODO: update to load all skyboxes maybe?
class Skybox {

public:
    struct PushConstants {
        glm::mat4 projectionView;
        VkDeviceAddress vertexBuffer;
    };

    Skybox();
    ~Skybox();

    void draw(VkCommandBuffer commandBuffer);
    void update(const glm::mat4& projectionView) {
        mPushConstants.projectionView = projectionView;
    }

    void drawGui();

private:
    void createPipelineLayout();
    void createPipeline();
    void createCubeMesh();
    void freeResources();

    AllocatedImage mTexture;
    MeshAsset mCubeMesh;

    PushConstants mPushConstants;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;

    std::string mChosenSkybox = "empty";
};