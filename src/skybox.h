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

    Skybox(std::string_view path);
    ~Skybox();

    void draw(VkCommandBuffer commandBuffer);
    void update(const glm::mat4& projectionView) {
        mPushConstants.projectionView = projectionView;
    }

private:
    void createPipelineLayout();
    void createPipeline();
    void createCubeMesh();
    void loadTexture(std::string_view path);
    void freeResources();

    AllocatedImage mTexture;
    MeshAsset mCubeMesh;

    PushConstants mPushConstants;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;
};