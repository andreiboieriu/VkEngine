#include "editor_app.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_scene.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "colors.h"

void EditorApp::parseCliArgs(const std::vector<std::string>& cliArgs) {
    mUseValidationLayers = false;

    for (auto& arg : cliArgs) {
        if (arg == "--debug") {
            mUseValidationLayers = true;
            fmt::println("Enabled validation layers");
        }
    }
}

void EditorApp::init(const std::vector<std::string>& cliArgs) {
    // parse cli args
    parseCliArgs(cliArgs);

    // create window
    mWindow = std::make_unique<Window>("Vulkan Engine", VkExtent2D{PROJ_SELECT_WINDOW_SIZE_X, PROJ_SELECT_WINDOW_SIZE_Y}, *this);

    mMainDeletionQueue.push([&]() {
        mWindow = nullptr;
    });

    // init vulkan
    initVulkan();
    initVMA();
    initSwapchain();
    initCommands();
    initSyncStructs();
    initImGui();
    initImages();

    mAssetManager = std::make_unique<AssetManager>(*this);
    mPipelineResourceManager = std::make_unique<PipelineResourceManager>(*this);
    mComputeEffectsManager = std::make_unique<ComputeEffectsManager>(*this);
    mSceneManager = std::make_unique<SceneManager>(*this);
    mScriptManager = std::make_unique<ScriptManager>(*this);

    mMainDeletionQueue.push([&]() {
        mPipelineResourceManager = nullptr;
        mAssetManager = nullptr;
        mComputeEffectsManager = nullptr;
        mScriptManager = nullptr;
        mSceneManager = nullptr;
    });

    mState = State::PROJECT_SELECTION;
}

void EditorApp::draw() {
    // get start time
    auto start = std::chrono::system_clock::now();

    // wait until gpu finishes rendering last frame
    VK_CHECK(vkWaitForFences(mDevice, 1, &getCurrentFrame().renderFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(mDevice, 1, &getCurrentFrame().renderFence));

    // flush deletion queue of current frame
    getCurrentFrame().deletionQueue.flush();

    VkImage swapchainImage = mWindow->getNextSwapchainImage(getCurrentFrame().swapchainSemaphore);

    if (swapchainImage == VK_NULL_HANDLE) {
        vkDestroySemaphore(mDevice, getCurrentFrame().swapchainSemaphore, nullptr);
        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &getCurrentFrame().swapchainSemaphore));

        return;
    }

    // set draw extent
    mDrawExtent.width = std::min(mManualDrawExtent.width * mEngineConfig.renderScale, 1.f * mDrawImage.imageExtent.width);
    mDrawExtent.height = std::min(mManualDrawExtent.height * mEngineConfig.renderScale, 1.f * mDrawImage.imageExtent.height);

    mRenderContext.drawExtent = glm::vec2(mDrawExtent.width, mDrawExtent.height);

    // get command buffer from current frame
    VkCommandBuffer commandBuffer = getCurrentFrame().mainCommandBuffer;

    // reset command buffer
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // begin recording command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    if (mState == State::EDITOR && mSceneManager && mSceneManager->hasActiveScene()) {
        vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        drawGeometry(commandBuffer);

        // transition draw image to general layout for adding post-processing
        vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_DEPTH_BIT);

        // execute post processing effects
        {
            // get start time
            auto start = std::chrono::system_clock::now();

            mComputeEffectsManager->executeEffects(commandBuffer, getComputeContext());

            auto end = std::chrono::system_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            mStats.postEffectsTimeBuffer += elapsed.count() / 1000.f;
        }

        // transition draw and swapchain image into transfer layouts
        vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
        vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // copy draw image to swapchain image
        vkutil::copyImageToImage(commandBuffer, mDrawImage.image, swapchainImage, mDrawExtent, {DRAW_EXTENT_X + DRAW_EXTENT_START_X, DRAW_EXTENT_Y + DRAW_EXTENT_START_Y}, {0, 0}, {DRAW_EXTENT_START_X, DRAW_EXTENT_START_Y});

        vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        // transition swapchain image to Attachment Optimal so we can draw directly to it
        vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    // draw ImGui into the swapchain image
    drawImGui(commandBuffer, mWindow->getCurrentSwapchainImageView());

    // transition swapchain image into a presentable layout
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // prepare to submit command buffer to queue
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(commandBuffer);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

    // submit command buffer to queue
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

    mWindow->presentSwapchainImage(mGraphicsQueue, getCurrentFrame().renderSemaphore);

    // increment frame number
    mFrameNumber++;
    mFrameNumber %= MAX_FRAMES_IN_FLIGHT;

    // get end time
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.drawTimeBuffer += elapsed.count() / 1000.f;
}

void EditorApp::update() {
    // get start time
    auto start = std::chrono::system_clock::now();

    // enable/disable scene camera
    const auto& input = mWindow->getInput();

    if (!mViewingScene && input.mouse.position.x > DRAW_EXTENT_START_X &&
        input.mouse.position.x < DRAW_EXTENT_X + DRAW_EXTENT_START_X &&
        input.mouse.position.y > DRAW_EXTENT_START_Y &&
        input.mouse.position.y < DRAW_EXTENT_Y + DRAW_EXTENT_START_Y &&
        input.mouse.buttonStates.at(GLFW_MOUSE_BUTTON_RIGHT) == InputState::HELD) {
        mViewingScene = true;
        mWindow->lockCursor();
    } else if (mViewingScene && input.mouse.buttonStates.at(GLFW_MOUSE_BUTTON_RIGHT) == InputState::RELEASED) {
        mViewingScene = false;
        mEditorCamera->resetVelocity();
        mWindow->unlockCursor();
    }

    if (mViewingScene && mSceneState == SceneState::STOPPED) {
        mEditorCamera->update(mDeltaTime, DRAW_EXTENT_RATIO, input);
    }

    if (mSceneManager && mSceneManager->hasActiveScene()) {
        if (mSceneState == SceneState::PLAYING) {
            mSceneManager->getActiveScene().update(mDeltaTime, input);

            if (mWindow->getInput().isKeyPressed(GLFW_KEY_ESCAPE)) {
                mWindow->unlockCursor();
            }
        } else if (mSceneState == SceneState::STOPPED) {
            mSceneManager->getActiveScene().editorModeUpdate(*mEditorCamera);
        }

        mSceneManager->getActiveScene().render(mRenderContext);
    }

    // get end time
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.updateTimeBuffer += elapsed.count() / 1000.f;
}

void EditorApp::initImGui() {
    // create descriptor pool for Imgui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiDescPool;
    VK_CHECK(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &imguiDescPool));

    // initialize imgui library
    ImGui::CreateContext();
    mWindow->initImGuiGLFW();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = mInstance;
    initInfo.PhysicalDevice = mPhysicalDevice;
    initInfo.Device = mDevice;
    initInfo.Queue = mGraphicsQueue;
    initInfo.DescriptorPool = imguiDescPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    VkFormat swapchainImageFormat = mWindow->getSwapchainImageFormat();

    // dynamic rendering parameters
    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    // add imgui resources to deletion queue
    mMainDeletionQueue.push([=, this]() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(mDevice, imguiDescPool, nullptr);
    });

    // load fonts
    auto io = ImGui::GetIO();
    mImGuiContext.smallFont = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Light.ttf", 12.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    mImGuiContext.normalFont = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    mImGuiContext.font12 = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 14.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    mImGuiContext.titleFont = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Bold.ttf", 32.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    // convert colors
    ImGuiStyle& style = ImGui::GetStyle();

    for (int i = 0; i < ImGuiCol_COUNT; ++i) {
        style.Colors[i] = sRGBtoLinear(style.Colors[i]);
    }

    mImGuiContext.spacing = ImGui::GetStyle().ItemSpacing;
}

std::filesystem::path openFolderDialog() {
    FILE* pipe = popen("kdialog --getexistingdirectory", "r");
    std::string folder = "";

    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            folder = buffer;
            folder.erase(std::remove(folder.begin(), folder.end(), '\n'), folder.end());
        }
        pclose(pipe);
    }

    return folder;
}

void EditorApp::createNewProject() {
    namespace fs = std::filesystem;

    auto folder = openFolderDialog();

    if (folder == "" || !fs::exists(folder)) {
        return;
    }

    // find available new project name
    std::string newPath = folder.string() + "/NewProject";
    int count = 1;

    while (fs::exists(newPath)) {
        if (count > 1) {
            newPath = newPath.substr(0, newPath.size() - std::to_string(count - 1).length());
        }

        newPath += std::to_string(count++);
    }

    // create default project
    if (!fs::create_directory(newPath)) {
        fmt::println("Failed to create new project dir: {}", newPath);
        exit(EXIT_FAILURE);
    }

    mProjectPath = newPath;

    bool created = true;
    created &= fs::create_directory(newPath + "/assets");
    created &= fs::create_directory(newPath + "/assets/gltfs");
    created &= fs::create_directory(newPath + "/assets/scenes");
    created &= fs::create_directory(newPath + "/assets/skyboxes");
    created &= fs::create_directory(newPath + "/assets/sprites");
    created &= fs::create_directory(newPath + "/assets/scripts");

    fs::copy("./assets/compute_effects", newPath + "/assets/compute_effects", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::copy("./shaders", newPath + "/shaders", fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    fs::copy("./assets/skyboxes/brdf_lut.ktx2", newPath + "/assets/skyboxes", fs::copy_options::overwrite_existing);
    fs::copy("./bin/game", newPath, fs::copy_options::overwrite_existing);

    // load project
    loadProject();
}

void EditorApp::openExistingProject() {
    auto folder = openFolderDialog();

    if (folder == "" || !std::filesystem::exists(folder)) {
        return;
    }

    // TODO: check this is an actual project
    mProjectPath = folder;

    // load project
    loadProject();
}

void EditorApp::loadProject() {
    parseConfig();

    mAssetManager->setProjectPath(mProjectPath);
    mAssetManager->loadAllAssets();

    mComputeEffectsManager->setProjectPath(mProjectPath);
    mComputeEffectsManager->loadEffects();

    mScriptManager->setProjectPath(mProjectPath);
    mScriptManager->loadAllScripts();

    mSceneManager->setProjectPath(mProjectPath);
    mSceneManager->init();

    mEditorCamera = std::make_unique<EditorCamera>(DRAW_EXTENT_RATIO);

    mMainDeletionQueue.push([&]() {
        mSceneManager = nullptr;
        mEditorCamera = nullptr;
    });

    mWindow->resize(EDITOR_WINDOW_SIZE_X, EDITOR_WINDOW_SIZE_Y, true);
    mManualDrawExtent.width = DRAW_EXTENT_X;
    mManualDrawExtent.height = DRAW_EXTENT_Y;

    mState = State::EDITOR;
    mSceneState = SceneState::STOPPED;
}

void EditorApp::drawProjectSelection() {
    auto viewport = ImGui::GetMainViewport();
    ImVec2 windowSize = viewport->Size;
    ImVec2 windowPos = viewport->Pos;
    const ImVec2 UISize = ImVec2(1020 - 720, windowSize.y);

    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(windowSize);

    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    auto backgroundImage = mAssetManager->getImGuiImage("proj-select-background.jpg");
    float backgroundAspect = 1.f * backgroundImage->image.imageExtent.width / backgroundImage->image.imageExtent.height;
    float targetAspect = 1.f;
    ImVec2 backgroundUV0{};
    ImVec2 backgroundUV1{};

    if (backgroundAspect > targetAspect) {
        float newWidth = targetAspect / backgroundAspect;
        float xOffset = (1.0f - newWidth) * 0.5f;
        backgroundUV0 = ImVec2(xOffset, 0.0f);
        backgroundUV1 = ImVec2(1.0f - xOffset, 1.0f);
    } else {
        float newHeight = backgroundAspect / targetAspect;
        float yOffset = (1.0f - newHeight) * 0.5f;
        backgroundUV0 = ImVec2(0, yOffset);
        backgroundUV1 = ImVec2(1, 1 - yOffset);
    }

    auto drawList = ImGui::GetBackgroundDrawList();
    drawList->AddImage(backgroundImage->id, ImVec2(0, 0), ImVec2(windowSize.x - UISize.x, windowSize.y), backgroundUV0, backgroundUV1);

    ImGui::SetNextWindowPos(ImVec2(720, 0));
    ImGui::SetNextWindowSize(UISize);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BACKGROUND_COLOR);
    ImGui::Begin("Project Selection", nullptr, windowFlags);

    // title
    ImGui::PushFont(mImGuiContext.titleFont);
    ImVec2 textSize = ImGui::CalcTextSize("Vulkan Engine");
    ImGui::SetCursorPos(ImVec2((UISize.x - textSize.x) * 0.5, 80));
    ImGui::TextUnformatted("Vulkan Engine");
    ImGui::PopFont();

    const ImVec2 buttonSize = ImVec2(260, 60);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16, 12)); // Internal padding

    ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);  // Normal
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    // new project button
    ImGui::PushFont(mImGuiContext.normalFont);
    ImGui::SetCursorPos(ImVec2((UISize.x - buttonSize.x) * 0.5, 80 + textSize.y + 80));
    if (ImGui::Button("New Project", buttonSize)) {
        createNewProject();
    }

    // open project button
    ImGui::SetCursorPos(ImVec2((UISize.x - buttonSize.x) * 0.5, 80 + textSize.y + 80 + 15 + 60));

    if (ImGui::Button("Open Project", buttonSize)) {
        openExistingProject();
    }

    ImGui::PopFont();

    // version text
    ImGui::PushFont(mImGuiContext.smallFont);
    textSize = ImGui::CalcTextSize("v0.1.0");
    ImGui::PopStyleColor();
    ImGui::SetCursorPos(ImVec2(300 - 20 - textSize.x, 720 - 20 - textSize.y));
    ImGui::TextUnformatted("v0.1.0");
    ImGui::PopFont();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    ImGui::End();
};

void EditorApp::drawToolbar(ImGuiWindowFlags windowFlags) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(1600, 40));

    ImGui::Begin("Toolbar", nullptr, windowFlags);

    ImGui::PushFont(mImGuiContext.font12);

    ImVec2 textSize = ImGui::CalcTextSize("FPS:");
    ImGui::SetCursorPosY((40 - textSize.y) / 2.f);

    std::string fps = "FPS: " + std::to_string(mStats.fps);
    ImGui::TextUnformatted(fps.c_str());

    std::string triangles = "Triangles drawn: " + std::to_string(mStats.triangleCount);
    ImGui::SameLine();
    ImGui::SetCursorPosY((40 - textSize.y) / 2.f);
    ImGui::TextUnformatted(triangles.c_str());

    std::string drawCalls = "Draw calls: " + std::to_string(mStats.drawCallCount);
    ImGui::SameLine();
    ImGui::SetCursorPosY((40 - textSize.y) / 2.f);
    ImGui::TextUnformatted(drawCalls.c_str());

    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners

    ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);  // Normal
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover

    auto playIcon = mAssetManager->getImGuiImage("play.png");
    auto pauseIcon = mAssetManager->getImGuiImage("pause.png");
    auto stopIcon = mAssetManager->getImGuiImage("stop.png");

    ImVec2 iconSize = ImVec2(25, 25);
    ImVec2 framePadding = ImGui::GetStyle().FramePadding;

    ImVec2 min = ImVec2(250 + (1000 - mImGuiContext.spacing.x * 2 - (iconSize.x + 2 * framePadding.x) * 3) / 2.0,  (40 - 2 * framePadding.y - iconSize.y) / 2.0);
    ImGui::SetCursorScreenPos(min);

    if (mSceneState == SceneState::PLAYING) {
        ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
    }

    bool play = ImGui::ImageButton("play", playIcon->id, iconSize);

    if (mSceneState == SceneState::PLAYING) {
        ImGui::PopStyleColor();
    }

    if (play) {
        if (mSceneState == SceneState::STOPPED) {
            // backup here
            mSceneManager->saveToFile();
            mImGuiContext.firstScene = mSceneManager->getActiveSceneName();

            mImGuiContext.visitedScenes.clear();
            mImGuiContext.visitedScenes.insert(mImGuiContext.firstScene);

            mSceneState = SceneState::PLAYING;
        } else if (mSceneState == SceneState::PAUSED) {
            mSceneState = SceneState::PLAYING;
        }
    }

    ImGui::SameLine();

    if (mSceneState == SceneState::PAUSED) {
        ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
    }

    bool pause = ImGui::ImageButton("pause", pauseIcon->id, iconSize);

    if (mSceneState == SceneState::PAUSED) {
        ImGui::PopStyleColor();
    }

    if (pause) {
        if (mSceneState == SceneState::PLAYING) {
            mSceneState = SceneState::PAUSED;
        }
    }

    ImGui::SameLine();

    if (mSceneState == SceneState::STOPPED) {
        ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
    }

    bool stop = ImGui::ImageButton("stop", stopIcon->id, iconSize);

    if (mSceneState == SceneState::STOPPED) {
        ImGui::PopStyleColor();
    }

    if (stop) {
        if (mSceneState == SceneState::PLAYING || mSceneState == SceneState::PAUSED) {
            mSceneState = SceneState::STOPPED;

            // restore backup here
            mSceneManager->restoreScenes(mImGuiContext);
        }
    }


    auto refreshIcon = mAssetManager->getImGuiImage("refresh.png");
    auto saveIcon = mAssetManager->getImGuiImage("diskette.png");

    min = ImVec2(1600 - mImGuiContext.spacing.x * 2 - (iconSize.x + 2 * framePadding.x) * 2, (40 - 2 * framePadding.y - iconSize.y) / 2.0);
    ImGui::SetCursorScreenPos(min);

    if (ImGui::ImageButton("refresh", refreshIcon->id, iconSize)) {
        mScriptManager->refreshScripts();
        mSceneManager->refreshScripts();
    }

    ImGui::SameLine();
    if (ImGui::ImageButton("save", saveIcon->id, iconSize)) {
        mSceneManager->saveToFile();
        saveConfigToFile();
    }

    ImGui::PopStyleVar(1);
    ImGui::PopStyleColor(3);
    ImGui::End();
}

void EditorApp::drawSceneHierachy(ImGuiWindowFlags windowFlags) {
    ImGui::SetNextWindowPos(ImVec2(0, 40));
    ImGui::SetNextWindowSize(ImVec2(250, 860 - 298));

    ImGui::Begin("Scene hierarchy", nullptr, windowFlags);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16, 12)); // Internal padding

    // ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);  // Normal
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushFont(mImGuiContext.normalFont);

    ImVec2 buttonSize = ImVec2(250 - 2 * mImGuiContext.spacing.x, 40);
    ImGui::Button("Scene hierarchy", buttonSize);

    ImGui::PopFont();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    if (mSceneManager->hasActiveScene()) {
        mSceneManager->getActiveScene().drawHierarchy(mImGuiContext);
    }
    ImGui::PopStyleVar();

    ImGui::End();
}

void EditorApp::drawInspector(ImGuiWindowFlags windowFlags) {

    ImGui::SetNextWindowPos(ImVec2(1250, 40));
    ImGui::SetNextWindowSize(ImVec2(350, 860));

    ImGui::Begin("Inspector", nullptr, windowFlags);

    ImGui::PushFont(mImGuiContext.normalFont);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16, 12)); // Internal padding

    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    enum class ColType : uint8_t {
        INSPECTOR,
        SCENE,
        POSTFX,
    };

    std::unordered_map<ColType, std::string> buttonNames;
    buttonNames[ColType::SCENE] = "Scene";
    buttonNames[ColType::INSPECTOR] = "Inspector";
    buttonNames[ColType::POSTFX] = "Post FX";

    const uint32_t buttonCount = 3;
    ImVec2 spacing = mImGuiContext.spacing;
    float buttonWidth = (350 - (buttonCount + 1) * spacing.x) / buttonCount;
    static ColType selectedButton = ColType::INSPECTOR;
    const ImVec2 buttonSize = ImVec2(buttonWidth, 40);

    for (uint8_t i = 0; i < 3; i++) {
        // If this button is the selected one, change its color or style to show selection
        if (i == (uint8_t)selectedButton)
            ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
        else
            ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);

        // Button label can be anything, here just "Button 0", "Button 1", ...
        if (ImGui::Button(buttonNames[ColType(i)].c_str(), buttonSize)) {
            selectedButton = ColType(i);
        }

        ImGui::PopStyleColor();

        // Put buttons on the same line (optional)
        if (i < 2)
            ImGui::SameLine();
    }

    ImGui::PopStyleVar(2);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    switch(selectedButton) {
    case ColType::SCENE:
        if (mSceneManager->hasActiveScene())
            mSceneManager->getActiveScene().drawConfig(mImGuiContext);
        break;
    case ColType::INSPECTOR:
        if (mSceneManager->hasActiveScene())
            mSceneManager->getActiveScene().drawInspector(mImGuiContext);
        break;
    case ColType::POSTFX:
        mComputeEffectsManager->drawGui(mImGuiContext);
        break;
    default:
        break;
    };

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::PopFont();

    ImGui::End();
}

void EditorApp::drawAssets(ImGuiWindowFlags windowFlags) {
    ImGui::SetNextWindowPos(ImVec2(250, 602));
    ImGui::SetNextWindowSize(ImVec2(1000, 298));

    ImGui::Begin("Assets", nullptr, windowFlags);

    ImGui::PushFont(mImGuiContext.normalFont);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16, 12)); // Internal padding

    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover

    enum class AssetType : uint8_t {
        SCENE,
        GLTF,
        SPRITE,
        SCRIPT,
        SKYBOX
    };

    std::unordered_map<AssetType, std::string> buttonNames;
    buttonNames[AssetType::SCENE] = "Scenes";
    buttonNames[AssetType::GLTF] = "glTFs";
    buttonNames[AssetType::SPRITE] = "Sprites";
    buttonNames[AssetType::SCRIPT] = "Scripts";
    buttonNames[AssetType::SKYBOX] = "Skyboxes";

    const uint32_t buttonCount = 5;
    ImVec2 spacing = mImGuiContext.spacing;
    float buttonWidth = (1000 - (buttonCount + 1) * spacing.x) / buttonCount;
    static AssetType selectedButton = AssetType::SCENE;
    const ImVec2 buttonSize = ImVec2(buttonWidth, 40);

    for (uint8_t i = 0; i < 5; i++) {
        // If this button is the selected one, change its color or style to show selection
        if (i == (uint8_t)selectedButton)
            ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
        else
            ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);

        // Button label can be anything, here just "Button 0", "Button 1", ...
        if (ImGui::Button(buttonNames[AssetType(i)].c_str(), buttonSize)) {
            selectedButton = AssetType(i);
        }

        ImGui::PopStyleColor();

        // Put buttons on the same line (optional)
        if (i < 4)
            ImGui::SameLine();
    }

    mImGuiContext.changedScene = false;

    switch(selectedButton) {
    case AssetType::SCENE:
        mSceneManager->drawSceneAssets(mImGuiContext);
        break;
    case AssetType::GLTF:
        mAssetManager->drawGltfAssets(mImGuiContext);
        break;
    case AssetType::SCRIPT:
        mScriptManager->drawScriptAssets(mImGuiContext);
        break;
    case AssetType::SPRITE:
        mAssetManager->drawSpriteAssets(mImGuiContext);
        break;
    case AssetType::SKYBOX:
        mAssetManager->drawSkyboxAssets(mImGuiContext);
        break;
    default:
        break;
    };

    if (mImGuiContext.changedScene == true) {
        mEditorCamera->reset();
        mImGuiContext.changedScene = false;
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    auto& droppedPaths = mWindow->getDroppedPaths();

    for (auto& path : droppedPaths) {
        std::filesystem::path filePath(path);

        if (!std::filesystem::exists(filePath) ||
            std::filesystem::is_directory(path) ||
            !filePath.has_extension()) continue;

        std::string extension = filePath.extension();

        if (extension == ".jpg" || extension == ".png") {
            std::filesystem::path dest = mProjectPath.string() + "/assets/sprites/" + filePath.filename().string();
            std::filesystem::copy_file(filePath, dest);
            mAssetManager->loadImage(filePath.filename(), VK_FORMAT_R8G8B8A8_SRGB);
        } else if (extension == ".glb") {
            std::filesystem::path dest = mProjectPath.string() + "/assets/gltfs/" + filePath.filename().string();
            std::filesystem::copy_file(filePath, dest);
            mAssetManager->loadGltf(filePath.filename());
        } else if (extension == ".hdr") {
            std::filesystem::path dest = mProjectPath.string() + "/assets/skyboxes/" + filePath.filename().string();
            std::filesystem::copy_file(filePath, dest);
            mAssetManager->loadSkybox(filePath.filename());
        } else {
            fmt::println("Error: attempting to drag/drop unsupported file {}", filePath.string());
        }
    }

    droppedPaths.clear();

    ImGui::End();
}

void EditorApp::drawEngineConfig(ImGuiWindowFlags windowFlags) {
    ImGui::SetNextWindowPos(ImVec2(0, 900 - 298));
    ImGui::SetNextWindowSize(ImVec2(250, 298));

    ImGui::Begin("EngineConfig", nullptr, windowFlags);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);     // Rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(16, 12)); // Internal padding

    // ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);  // Normal
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Button, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);  // Hover
    ImGui::PushFont(mImGuiContext.normalFont);

    ImVec2 buttonSize = ImVec2(250 - 2 * mImGuiContext.spacing.x, 40);
    ImGui::Button("Engine config", buttonSize);
    ImGui::PopStyleVar(1);

    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);
    ImGui::PopFont();
    ImGui::PushFont(mImGuiContext.font12);

    const float maxScale = std::min(1.f * MAX_IMAGE_WIDTH / DRAW_EXTENT_X, 1.f * MAX_IMAGE_HEIGHT / DRAW_EXTENT_Y);

    ImGui::SliderFloat("Render scale", &mEngineConfig.renderScale, 0.1f, maxScale);
    ImGui::SliderFloat("Shadow depth", &mShadowDepth, 0.f, 10000.f);

    if (ImGui::Checkbox("VSync", &mEngineConfig.vsync)) {
        if (mEngineConfig.vsync) {
            mWindow->enableVsync();
        } else {
            mWindow->disableVsync();
        }
    }

    ImGui::Checkbox("Frustum culling", &mEngineConfig.enableFrustumCulling);
    ImGui::Checkbox("Draw sorting", &mEngineConfig.enableDrawSorting);

    static char entryScene[100] = {};
    strcpy(entryScene, mEngineConfig.entryPoint.c_str());
    if (ImGui::InputText("Entry scene", entryScene, 99)) {
        mEngineConfig.entryPoint = std::string(entryScene);
    }

    ImGui::PopFont();
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(1);

    ImGui::End();
}

void EditorApp::drawEditor() {
    ImGuiWindowFlags windowFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, BACKGROUND_COLOR);

    drawToolbar(windowFlags);
    drawSceneHierachy(windowFlags);
    drawInspector(windowFlags);
    drawAssets(windowFlags);
    drawEngineConfig(windowFlags);

    ImGui::PopStyleColor();
}

void EditorApp::drawGui() {
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (mState == State::PROJECT_SELECTION) {
        drawProjectSelection();
    } else if (mState == State::EDITOR) {
        drawEditor();
    }

    // ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    // if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    //     ImGui::Text("fps %i", mStats.fps);
    //     ImGui::Text("frametime %f ms", mStats.frameTime);
    //     ImGui::Text("update time %f ms", mStats.updateTime);
    //     ImGui::Text("draw time %f ms", mStats.drawTime);
    //     ImGui::Text("draw geometry time %f ms", mStats.drawGeometryTime);
    //     ImGui::Text("post effects time %f ms", mStats.postEffectsTime);
    //     ImGui::Text("triangles %i", mStats.triangleCount);
    //     ImGui::Text("draws %i", mStats.drawCallCount);
    //     ImGui::End();
    // }

    // if (!mWindow->isCursorLocked()) {
    //     ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    //     if (ImGui::Begin("Engine Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    //         ImGui::Checkbox("Enable frustum culling", &mEngineConfig.enableFrustumCulling);
    //         ImGui::Checkbox("Enable draw sorting", &mEngineConfig.enableDrawSorting);
    //         ImGui::SliderFloat("Render Scale", &mEngineConfig.renderScale, 0.3f, 2.f);

    //         ImGui::End();
    //     }

    //     // mScene->drawGui();
    //     mComputeEffectsManager->drawGui();
    //     mWindow->drawGui();
    // }

    // render ImGui
    ImGui::Render();
}
