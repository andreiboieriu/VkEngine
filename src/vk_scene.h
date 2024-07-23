#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
class Scene3D {

public:
    
    Scene3D(const std::string& name);

    void update(float dt);
    void draw(RenderContext &context);




private:

    std::string mName;
};