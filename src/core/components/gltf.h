#pragma once

struct LoadedGLTF;

struct GLTF {
    LoadedGLTF *gltf = nullptr;
    std::string name = "";
};
