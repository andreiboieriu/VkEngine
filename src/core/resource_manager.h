#pragma once

#include <unordered_map>
#include <vk_loader.h>
#include <memory.h>

class ResourceManager {

public:

    ResourceManager(VulkanEngine& vkEngine);
    ~ResourceManager();

    void loadGltf(std::filesystem::path filePath);
    void loadImage(std::filesystem::path filePath);

    LoadedGLTF *getGltf(std::string name) {
        if (!mLoadedGltfs.contains(name)) {
            return nullptr;
        }

        return mLoadedGltfs[name].get();
    }

    const AllocatedImage& getImage(std::string name) {
        return mImages[name];
    }

private:
    void loadResources();
    void freeResources();

    VulkanEngine& mVkEngine;

    std::unordered_map<std::string, std::unique_ptr<LoadedGLTF>> mLoadedGltfs;
    std::unordered_map<std::string, AllocatedImage> mImages;
};
