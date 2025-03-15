#pragma once

#include <unordered_map>
#include <vk_loader.h>
#include <memory.h>

class ResourceManager {

public:

    ResourceManager(VulkanEngine& vkEngine);
    ~ResourceManager();

    std::shared_ptr<LoadedGLTF> getGltf(std::string name) {
        if (!mLoadedGltfs.contains(name)) {
            return nullptr;
        }

        return mLoadedGltfs[name];
    }

    const AllocatedImage& getSkyboxCubemap(std::string name) {
        return mSkyboxCubemaps[name];
    }

private:
    void loadResources();
    void freeResources();

    VulkanEngine& mVkEngine;

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> mLoadedGltfs;
    std::unordered_map<std::string, AllocatedImage> mSkyboxCubemaps;
};
