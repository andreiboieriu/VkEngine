#include "resource_manager.h"
#include "stb_image.h"
#include "vk_engine.h"
#include "vk_loader.h"

ResourceManager::ResourceManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    loadResources();
}

ResourceManager::~ResourceManager() {
    freeResources();
}

void ResourceManager::loadResources() {
    // mLoadedGltfs["hull_spaceship"] = std::make_shared<LoadedGLTF>("assets/hull_spaceship.glb");
    // mLoadedGltfs["sci_fi_hangar"] = std::make_shared<LoadedGLTF>("assets/sci_fi_hangar.glb");
    mLoadedGltfs["fighter_spaceship"] = std::make_shared<LoadedGLTF>("assets/fighter_spaceship.glb", mVkEngine);
    // mLoadedGltfs["asteroid"] = std::make_shared<LoadedGLTF>("assets/asteroid2.glb");
}

void ResourceManager::freeResources() {
    for (auto& [k, v] : mLoadedGltfs) {
        v = nullptr;
    }

    for (auto& [k, v] : mSkyboxCubemaps) {
        mVkEngine.destroyImage(v);
    }
}
