#pragma once

struct AllocatedImage;

struct Sprite {
    AllocatedImage *image = nullptr;
    std::string name = "";
};
