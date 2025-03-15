#pragma once

#include "vk_engine.h"

class GameApp : public VulkanEngine {
public:


private:
    void draw() override;
	void drawGui() override;
	void update() override;
};
