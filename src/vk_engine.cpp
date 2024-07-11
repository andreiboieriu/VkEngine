//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_initializers.h"
#include "vk_types.h"

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

VulkanEngine* gLoadedEngine = nullptr;

VulkanEngine& VulkanEngine::get() { 
    return *gLoadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(gLoadedEngine == nullptr);
    gLoadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    mWindow = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        mWindowExtent.width,
        mWindowExtent.height,
        windowFlags);

    // initialize vulkan
    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructs();

    // everything went fine
    mIsInitialized = true;
}

void VulkanEngine::initVulkan() {
    vkb::InstanceBuilder builder{};

    // build vulkan instance using vkbootstrap
    vkb::Instance vkbInstance = builder.set_app_name("VkEngine")
        .request_validation_layers(mUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build()
        .value();

    // get instance and messenger from vkbInstance
    mInstance = vkbInstance.instance;
    mDebugMessenger = vkbInstance.debug_messenger;

    // create vulkan surface
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    // get needed vulkan features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // select gpu using vkbootstrap
    vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
    vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(mSurface)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select()
        .value();

    // create logical device using vkbootstrap
    vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

    // get device handles
    mDevice = vkbDevice.device;
    mPhysicalDevice = vkbPhysicalDevice.physical_device; 
}

void VulkanEngine::initSwapchain() {

}

void VulkanEngine::initCommands() {

}

void VulkanEngine::initSyncStructs() {

}

void VulkanEngine::cleanup()
{
    if (mIsInitialized) {
        SDL_DestroyWindow(mWindow);
    }

    // clear engine pointer
    gLoadedEngine = nullptr;
}

void VulkanEngine::draw()
{
    // nothing yet
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
            // close the window when user alt-f4s or clicks the X button
            case SDL_QUIT:
                bQuit = true;
                break;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    mStopRendering = true;
                }

                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    mStopRendering = false;
                }
                break;

            default:
                break;
            }
        }

        // do not draw if we are minimized
        if (mStopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}