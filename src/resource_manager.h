#pragma once

#include <unordered_map>
#include <vk_loader.h>
#include <memory.h>

class ResourceManager {

public:

    ResourceManager();
    ~ResourceManager();

private:
    void loadResources();
    void freeResources();

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> mLoadedGltfs;
};
