#include "editor_app.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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

    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(commandBuffer);

    // transition draw image to general layout for adding post-processing
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    // execute post processing effects
    {
        // get start time
        auto start = std::chrono::system_clock::now();

        mComputeEffectsManager->executeEffects(commandBuffer, mDrawImage, mDrawExtent);

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        mStats.postEffectsTimeBuffer += elapsed.count() / 1000.f;
    }

    // transition draw and swapchain image into transfer layouts
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy draw image to swapchain image
    vkutil::copyImageToImage(commandBuffer, mDrawImage.image, swapchainImage, mDrawExtent, windowExtent);

    // transition swapchain image to Attachment Optimal so we can draw directly to it
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

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

    // get end time
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.drawTimeBuffer += elapsed.count() / 1000.f;
}

void EditorApp::update() {
    // get start time
    auto start = std::chrono::system_clock::now();

    mScene->update(mDeltaTime, mWindow->getInput());
    mScene->render(mRenderContext);

    // get end time
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.updateTimeBuffer += elapsed.count() / 1000.f;
}

void EditorApp::drawGui() {
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("fps %i", mStats.fps);
        ImGui::Text("frametime %f ms", mStats.frameTime);
        ImGui::Text("update time %f ms", mStats.updateTime);
        ImGui::Text("draw time %f ms", mStats.drawTime);
        ImGui::Text("draw geometry time %f ms", mStats.drawGeometryTime);
        ImGui::Text("post effects time %f ms", mStats.postEffectsTime);
        ImGui::Text("triangles %i", mStats.triangleCount);
        ImGui::Text("draws %i", mStats.drawCallCount);
        ImGui::End();
    }


    if (!mWindow->isCursorLocked()) {
        ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Engine Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Checkbox("Enable frustum culling", &mEngineConfig.enableFrustumCulling);
            ImGui::Checkbox("Enable draw sorting", &mEngineConfig.enableDrawSorting);
            ImGui::SliderFloat("Render Scale", &mEngineConfig.renderScale, 0.3f, 2.f);

            ImGui::End();
        }

        mScene->drawGui();
        mWindow->drawGui();
    }

    // render ImGui
    ImGui::Render();
}
