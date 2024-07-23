#pragma once

#include "vk_loader.h"
#include <memory>
#include <string_view>

struct GLTF {
    std::shared_ptr<LoadedGLTF> gltf;

    GLTF(std::shared_ptr<LoadedGLTF> gltf) : gltf(gltf) {}
    GLTF(std::string_view filePath) {
        gltf = std::make_shared<LoadedGLTF>(filePath);
    }
};