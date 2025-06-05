#pragma once

#include "vk_engine.h"
#include <imgui.h>

class EditorApp : public VulkanEngine {
public:
    void init(const std::vector<std::string>& cliArgs) override;
    void parseCliArgs(const std::vector<std::string>& cliArgs) override;

private:
    enum class State : uint8_t {
        PROJECT_SELECTION,
        EDITOR,
    };

    void draw() override;
	void drawGui() override;
	void update() override;

	void drawProjectSelection();
	void initImGui() override;
	void createNewProject();

	State mState;

	ImFont* mNormalFont;
	ImFont* mTitleFont;
};
