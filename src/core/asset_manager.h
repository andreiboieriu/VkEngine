#pragma once

#include <unordered_map>
#include <vk_loader.h>
#include <memory.h>

class AssetManager {

public:
    AssetManager(VulkanEngine& vkEngine);
    ~AssetManager();

    void loadGltf(const std::string& name);
    void loadImage(const std::string& name);
    void loadSkybox(const std::string& name);

    LoadedGLTF *getGltf(const std::string& name) {
        if (!mLoadedGltfs.contains(name)) {
            return nullptr;
        }

        return mLoadedGltfs[name].get();
    }

    AllocatedImage *getImage(const std::string& name) {
        if (!mImages.contains(name)) {
            return nullptr;
        }

        return &mImages[name];
    }

    MeshAsset *getMesh(const std::string& name) {
        if (!mMeshes.contains(name)) {
            return nullptr;
        }

        return &mMeshes[name];
    }

    SkyboxAsset *getSkybox(const std::string& name) {
        if (!mSkyboxes.contains(name)) {
            return nullptr;
        }

        return &mSkyboxes[name];
    }

    VkSampler *getSampler(const std::string& name) {
        if (!mDefaultSamplers.contains(name)) {
            return nullptr;
        }

        return &mDefaultSamplers[name];
    }

    AllocatedImage *getDefaultImage(const std::string& name) {
        if (!mDefaultImages.contains(name)) {
            return nullptr;
        }

        return &mDefaultImages[name];
    }

private:
    void loadDefaultMeshes();
    void loadDefaultImages();
    void loadDefaultSamplers();
    void renderToCubemap(AllocatedImage& src, AllocatedImage& dest, Pipeline *pipeline, VkExtent2D destSize, uint32_t mipLevel = 0, bool flipViewport = false);
    void freeResources();

    VulkanEngine& mVkEngine;

    const std::string GLTFS_PATH = "assets/gltfs/";
    const std::string SPRITES_PATH = "assets/sprites/";
    const std::string SKYBOXES_PATH = "assets/skyboxes/";

    // user loaded data
    std::unordered_map<std::string, std::unique_ptr<LoadedGLTF>> mLoadedGltfs;
    std::unordered_map<std::string, AllocatedImage> mImages;
    std::unordered_map<std::string, SkyboxAsset> mSkyboxes;

    // default data
    std::unordered_map<std::string, MeshAsset> mMeshes;
    std::unordered_map<std::string, AllocatedImage> mDefaultImages;
    std::unordered_map<std::string, VkSampler> mDefaultSamplers;
};
