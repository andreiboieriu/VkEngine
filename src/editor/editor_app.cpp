#include "editor_app.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_scene.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
    mWindow = std::make_unique<Window>("Vulkan Engine", VkExtent2D{1280, 720}, *this);

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
    initECS();
    initImages();

    mAssetManager = std::make_unique<AssetManager>(*this);
    mPipelineResourceManager = std::make_unique<PipelineResourceManager>(*this);
    mComputeEffectsManager = std::make_unique<ComputeEffectsManager>(*this);

    mMainDeletionQueue.push([&]() {
        mPipelineResourceManager = nullptr;
        mAssetManager = nullptr;
        mComputeEffectsManager = nullptr;
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

    VkExtent2D windowExtent = mWindow->getExtent();

    // set draw extent
    mDrawExtent.width = std::min(windowExtent.width, mDrawImage.imageExtent.width) * mEngineConfig.renderScale;
    mDrawExtent.height = std::min(windowExtent.height, mDrawImage.imageExtent.height) * mEngineConfig.renderScale;

    // get command buffer from current frame
    VkCommandBuffer commandBuffer = getCurrentFrame().mainCommandBuffer;

    // reset command buffer
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // begin recording command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    // drawGeometry(commandBuffer);

    // // transition draw image to general layout for adding post-processing
    // vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    // // execute post processing effects
    // {
    //     // get start time
    //     auto start = std::chrono::system_clock::now();

    //     mComputeEffectsManager->executeEffects(commandBuffer, getComputeContext());

    //     auto end = std::chrono::system_clock::now();
    //     auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    //     mStats.postEffectsTimeBuffer += elapsed.count() / 1000.f;
    // }

    // // // transition draw and swapchain image into transfer layouts
    // vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    // vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // // // copy draw image to swapchain image
    // vkutil::copyImageToImage(commandBuffer, mDrawImage.image, swapchainImage, mDrawExtent, windowExtent);

    // // transition swapchain image to Attachment Optimal so we can draw directly to it
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

    if (mScene) {
        mScene->update(mDeltaTime, mWindow->getInput(), mFrameNumber);
        mScene->render(mRenderContext);
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
    mNormalFont = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    mTitleFont = io.Fonts->AddFontFromFileTTF("fonts/Roboto-Bold.ttf", 32.0f, NULL, io.Fonts->GetGlyphRangesDefault());

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();
}

std::filesystem::path dialog() {
    char filename[1024];
    FILE *f = popen("zenity --file-selection", "r");
    fgets(filename, 1024, f);
    filename[strlen(filename) - 1] = 0;
    std::string file(filename);
    fmt::println("File: {}", file);
}

void EditorApp::createNewProject() {
    dialog();
}

void EditorApp::drawProjectSelection() {
    auto viewport = ImGui::GetMainViewport();
    ImVec2 windowSize = viewport->Size;
    ImVec2 windowPos = viewport->Pos;

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

    ImGui::Begin("Project Selection", nullptr, windowFlags);

    ImGui::PushFont(mTitleFont);
    ImGui::SetCursorPosY((windowSize.y - 50.f) * 0.25f);
    ImGui::SetCursorPosX((1280.f - ImGui::CalcTextSize("Vulkan Game Engine").x) * 0.5f);
    ImGui::Text("Vulkan Game Engine");
    ImGui::PopFont();

    ImGui::PushFont(mNormalFont);
    ImGui::SetCursorScreenPos(ImVec2(400, (windowSize.y - 50.f) * 0.5f));

    // New Project
    if (ImGui::Button("New Project", ImVec2(200.f, 50.f))) {
        createNewProject();
    }

    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(680, (windowSize.y - 50.f) * 0.5f));

    // Open Project
    if (ImGui::Button("Open Project", ImVec2(200.f, 50.f))) {
        // Open ImGuiFileDialog or your own file selector
    }

    ImGui::PopFont();

    ImGui::End();
};

void EditorApp::drawGui() {
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawProjectSelection();

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
