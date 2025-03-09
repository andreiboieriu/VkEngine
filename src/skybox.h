#pragma once

#include "vk_types.h"
#include "vk_loader.h"

// TODO: update to load all skyboxes maybe?
class Skybox {

public:
    struct PushConstants {
        glm::mat4 projectionView;
        VkDeviceAddress vertexBuffer;
        uint32_t mipLevel;
        uint32_t totalMips;
    };

    Skybox(VulkanEngine& vkEngine);
    ~Skybox();

    void load(std::filesystem::path filePath);
    void draw(VkCommandBuffer commandBuffer);
    void update(const glm::mat4& projectionView) {
        mPushConstants.projectionView = projectionView;
    }

    const AllocatedImage& getIrradianceMap() {
        return mIrrMap;
    }

    const AllocatedImage& getPrefilteredEnvMap() {
        return mPrefilteredEnvMap;
    }

    const AllocatedImage& getBrdfLut() {
        return mBrdfLut;
    }

    void drawGui();

private:
    void createPipelineLayout();
    void createPipeline();
    void createCubeMesh();
    void freeResources();

    void renderToCubemap(AllocatedImage& src, AllocatedImage& dest, VkPipeline pipeline, VkExtent2D destSize, uint32_t mipLevel = 0, bool flipViewport = false);
    void renderBrdfLut();

    VulkanEngine& mVkEngine;

    AllocatedImage mHDRImage;
    AllocatedImage mEnvMap;
    AllocatedImage mIrrMap;
    AllocatedImage mPrefilteredEnvMap;
    AllocatedImage mBrdfLut;
    MeshAsset mCubeMesh;

    PushConstants mPushConstants;

    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;
    VkPipeline mEquiToCubePipeline;
    VkPipeline mEnvToIrrPipeline;
    VkPipeline mPrefilterEnvPipeline;

    const VkExtent2D ENV_MAP_SIZE = {1024, 1024};
    const VkExtent2D IRR_MAP_SIZE = {32, 32};
    const VkExtent2D PREFILTERED_ENV_MAP_SIZE = {256, 256};
    const VkExtent2D BRDF_LUT_SIZE = {512, 512};

    std::string mChosenSkybox = "empty";

    int mMipLevel = 0;
};
