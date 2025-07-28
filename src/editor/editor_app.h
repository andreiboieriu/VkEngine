#pragma once

#include "vk_engine.h"
#include <imgui.h>

#include "colors.h"
#include "editor_camera.h"

class EditorApp : public VulkanEngine {
public:
    void init(const std::vector<std::string>& cliArgs) override;
    void parseCliArgs(const std::vector<std::string>& cliArgs) override;

private:
    enum class State : uint8_t {
        PROJECT_SELECTION,
        EDITOR,
    };

    enum class SceneState : uint8_t {
        PLAYING,
        STOPPED,
        PAUSED
    };

    void draw() override;
	void drawGui() override;
	void update() override;

	void drawProjectSelection();
	void drawEditor();
	void drawToolbar(ImGuiWindowFlags windowFlags);
	void drawSceneHierachy(ImGuiWindowFlags windowFlags);
	void drawInspector(ImGuiWindowFlags windowFlags);
	void drawAssets(ImGuiWindowFlags windowFlags);
	void drawEngineConfig(ImGuiWindowFlags windowFlags);

	void initImGui() override;
	void createNewProject();
	void openExistingProject();

	void loadProject();
	State mState;
	SceneState mSceneState;

	bool mViewingScene = false;

	VkExtent2D mManualDrawExtent;

	std::unique_ptr<EditorCamera> mEditorCamera;

	const uint32_t PROJ_SELECT_WINDOW_SIZE_X = 1020;
	const uint32_t PROJ_SELECT_WINDOW_SIZE_Y = 720;
	const uint32_t EDITOR_WINDOW_SIZE_X = 1600;
	const uint32_t EDITOR_WINDOW_SIZE_Y = 900;
	const uint32_t DRAW_EXTENT_START_X = 250;
	const uint32_t DRAW_EXTENT_START_Y = 40;
	const uint32_t DRAW_EXTENT_X = 1000;
	const uint32_t DRAW_EXTENT_Y = 562;

	const float DRAW_EXTENT_RATIO = 1.f * DRAW_EXTENT_X / DRAW_EXTENT_Y;
};
