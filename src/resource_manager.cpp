#include "resource_manager.h"
#include "stb_image.h"
#include "vk_engine.h"

ResourceManager::ResourceManager() {
    loadResources();
}

ResourceManager::~ResourceManager() {
    freeResources();
}

void ResourceManager::loadResources() {
    // mLoadedGltfs["hull_spaceship"] = std::make_shared<LoadedGLTF>("assets/hull_spaceship.glb");
    // mLoadedGltfs["hull_spaceship"] = std::make_shared<LoadedGLTF>("assets/MetalRoughSpheres.gltf");
    mLoadedGltfs["fighter_spaceship"] = std::make_shared<LoadedGLTF>("assets/fighter_spaceship.glb");

    mSkyboxCubemaps["nebula"] = loadSkyboxCubemap("assets/skybox/nebula/");
    mSkyboxCubemaps["anime"] = loadSkyboxCubemap("assets/skybox/anime/");
}

void ResourceManager::freeResources() {
    for (auto& [k, v] : mLoadedGltfs) {
        v = nullptr;
    }

    for (auto& [k, v] : mSkyboxCubemaps) {
        VulkanEngine::get().destroyImage(v);
    }
}

AllocatedImage ResourceManager::loadSkyboxCubemap(std::string_view path) {
    AllocatedImage newCubemap;

    int width, height, nrChannels;

    std::vector<std::string> filePaths;
    filePaths.push_back(std::string(path) + "px.png");
    filePaths.push_back(std::string(path) + "nx.png");
    filePaths.push_back(std::string(path) + "py.png");
    filePaths.push_back(std::string(path) + "ny.png");
    filePaths.push_back(std::string(path) + "pz.png");
    filePaths.push_back(std::string(path) + "nz.png");

    void* data[6]{};

    for (int i = 0; i < 6; i++) {
        data[i] = stbi_load(filePaths[i].c_str(), &width, &height, &nrChannels, 4);

        if (!data[i]) {
            fmt::println("failed to load image with stbi: {}", filePaths[i]);
        }
    }

    newCubemap = VulkanEngine::get().createSkybox(data, VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)}, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);

    for (int i = 0; i < 6; i++) {
        stbi_image_free(data[i]);
    }

    return newCubemap;
}
