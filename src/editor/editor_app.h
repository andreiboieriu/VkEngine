#pragma once

#include "vk_engine.h"

class EditorApp : public VulkanEngine {
public:


private:
    void draw() override;
	void drawGui() override;
	void update() override;
};
