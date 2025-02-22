#include "vk_swapchain.h"

#include <VkBootstrap.h>
#include "vk_engine.h"
#include "vk_initializers.h"

Swapchain::Swapchain(VkExtent2D extent, VkPresentModeKHR presentMode, VkSurfaceKHR surface) : mExtent(extent) {
    init(presentMode, surface);
}

Swapchain::~Swapchain() {
    VkDevice device = VulkanEngine::get().getDevice();

    // destroy swapchain resources
    for (auto& imageView : mImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, mHandle, nullptr);
}

void Swapchain::init(VkPresentModeKHR presentMode, VkSurfaceKHR surface) {
    // set swapchain image format
    mImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // build swapchain using vkb
    vkb::SwapchainBuilder vkbSwapchainBuilder{
        VulkanEngine::get().getPhysicalDevice(),
        VulkanEngine::get().getDevice(),
        surface
    };

    VkSurfaceFormatKHR surfaceFormat{};
    surfaceFormat.format = mImageFormat;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(presentMode) // vsync present mode
        // .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
        .set_desired_min_image_count(vkb::SwapchainBuilder::DOUBLE_BUFFERING)
        .set_desired_extent(mExtent.width, mExtent.height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    // get swapchain and related images from vkbSwapchain
    mHandle = vkbSwapchain.swapchain;

    mImages = vkbSwapchain.get_images().value();
    mImageViews = vkbSwapchain.get_image_views().value();
}

VkImage Swapchain::getNextImage(VkSemaphore& semaphore) {
     // request image from swapchain
    VkResult e = vkAcquireNextImageKHR(
        VulkanEngine::get().getDevice(),
        mHandle,
        UINT64_MAX,
        semaphore,
        VK_NULL_HANDLE,
        &mCurrentImageIndex
    );

    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        // vkDestroySemaphore(VulkanEngine::get().getDevice(), semaphore, nullptr);
        // VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
        // VK_CHECK(vkCreateSemaphore(VulkanEngine::get().getDevice(), &semaphoreCreateInfo, nullptr, &semaphore));
    
        return VK_NULL_HANDLE;
    } else if (e != VK_SUCCESS) {
        fmt::println("Failed to acquire swapchain image\n");
        exit(EXIT_FAILURE);
    }

    return mImages[mCurrentImageIndex];
}

bool Swapchain::presentImage(VkQueue graphicsQueue, VkSemaphore waitSemaphore) {
    // present image
    VkPresentInfoKHR presentInfo = vkinit::present_info(&mHandle, &waitSemaphore, &mCurrentImageIndex);

    VkResult e = vkQueuePresentKHR(graphicsQueue, &presentInfo);

    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        return false;
    } else if (e != VK_SUCCESS)  {
        fmt::println("Failed to present swapchain image\n");
        exit(EXIT_FAILURE);
    }

    return true;
}