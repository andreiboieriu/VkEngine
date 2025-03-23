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

void ResourceManager::loadImage(std::filesystem::path filePath) {

}

void ResourceManager::loadGltf(std::filesystem::path filePath) {
    // check if file exists
    assert(std::filesystem::exists(filePath) && ("Failed to find gltf file: " + filePath.string()).c_str());

    fmt::println("Loading GLTF: {}", filePath.string());

    auto assetName = filePath.stem().generic_string();

    mLoadedGltfs[assetName] = std::make_unique<LoadedGLTF>(filePath, mVkEngine);
}

void ResourceManager::loadResources() {
    // mLoadedGltfs["hull_spaceship"] = std::make_shared<LoadedGLTF>("assets/hull_spaceship.glb");
    // mLoadedGltfs["sci_fi_hangar"] = std::make_shared<LoadedGLTF>("assets/sci_fi_hangar.glb");
    loadGltf("assets/fighter_spaceship.glb");
    // mLoadedGltfs["asteroid"] = std::make_shared<LoadedGLTF>("assets/asteroid2.glb");
}

void ResourceManager::freeResources() {
    for (auto& [k, v] : mLoadedGltfs) {
        v = nullptr;
    }

    for (auto& [k, v] : mImages) {
        mVkEngine.destroyImage(v);
    }
}
