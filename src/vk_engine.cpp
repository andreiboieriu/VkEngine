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

    // get graphics queue using vkb
    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initSwapchain() {
    createSwapchain(mWindowExtent.width, mWindowExtent.height);
}

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    // set swapchain image format
    mSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // build swapchain using vkb
    vkb::SwapchainBuilder vkbSwapchainBuilder{mPhysicalDevice, mDevice, mSurface};

    VkSurfaceFormatKHR surfaceFormat{};
    surfaceFormat.format = mSwapchainImageFormat;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync present mode
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    // get swapchain and related images from vkbSwapchain
    mSwapchain = vkbSwapchain.swapchain;
    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroySwapchain() {
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

    // destroy swapchain resources
    for (auto& imageView : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }
}

void VulkanEngine::initCommands() {
    // create command pool for graphics queue commands
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        mGraphicsQueueFamily,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        );

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mFrames[i].commandPool));

        // allocate command buffer from pool
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(
            mFrames[i].commandPool,
            1
            );

        VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mFrames[i].mainCommandBuffer));
    }
}

void VulkanEngine::initSyncStructs() {

}

void VulkanEngine::cleanup()
{
    if (!mIsInitialized) {
        return;
    }

    // wait until gpu stops
    vkDeviceWaitIdle(mDevice);

    // free command pools
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(mDevice, mFrames[i].commandPool, nullptr);
    }

    destroySwapchain();

    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);

    vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
    vkDestroyInstance(mInstance, nullptr);

    SDL_DestroyWindow(mWindow);

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