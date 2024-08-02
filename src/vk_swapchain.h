#pragma once

#include "volk.h"

class Swapchain {

public:
    Swapchain(VkExtent2D extent, VkPresentModeKHR presentMode, VkSurfaceKHR surface);
    ~Swapchain();

    VkSwapchainKHR getHandle() {
        return mHandle;
    }

    VkImage getNextImage(VkSemaphore& semaphore);
    VkImageView getCurrentImageView() {
        return mImageViews[mCurrentImageIndex];
    }

    VkFormat getImageFormat() {
        return mImageFormat;
    }

    bool presentImage(VkQueue graphicsQueue, VkSemaphore waitSemaphore);

private:
    void init(VkPresentModeKHR presentMode, VkSurfaceKHR surface);

    VkExtent2D mExtent;
    VkFormat mImageFormat;
    VkSwapchainKHR mHandle;

    std::vector<VkImage> mImages;
	std::vector<VkImageView> mImageViews;

    uint32_t mCurrentImageIndex;
};
