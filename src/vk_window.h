#pragma once

#include <string_view>

#include <SDL.h>
#include "volk.h"

#include "vk_swapchain.h"
#include <set>

struct UserInput {
    std::set<SDL_Keycode> pressedKeys;
    std::set<SDL_Keycode> releasedKeys;
    float mouseXRel;
    float mouseYRel;
    float mouseWheel;
    bool GuiMode;
};

class Window {

public:
    Window(std::string_view name, VkExtent2D extent, SDL_WindowFlags windowFlags, uint32_t monitorIdx);
    ~Window();

    bool shouldClose() {
        return mShouldClose;
    }

    bool isMinimized() {
        return mIsMinimized;
    }

    VkExtent2D getExtent() {
        return mExtent;
    }

    float getAspectRatio() {
        return (float)mExtent.width / (float)mExtent.height;
    }

    void createSwapchain();

    VkSwapchainKHR getSwapchainHandle() {
        return mSwapchain->getHandle();
    }

    VkImageView getCurrentSwapchainImageView() {
        return mSwapchain->getCurrentImageView();
    }

    VkFormat getSwapchainImageFormat() {
        return mSwapchain->getImageFormat();
    }

    VkSurfaceKHR getSurface();

    VkImage getNextSwapchainImage(VkSemaphore& semaphore);

    void presentSwapchainImage(VkQueue graphicsQueue, VkSemaphore waitSemaphore);

    UserInput processSDLEvents();

    void toggleLockedCursor();

    void update();

    struct SDL_Window* getHandle() {
        return mHandle;
    }

private:
    void init(SDL_WindowFlags windowFlags, uint32_t monitorIdx);
    void handleWindowEvent(SDL_WindowEventID event);

    void updateExtent();
    void resize();

    const std::string mName;
    VkExtent2D mExtent;

	// sdl window handle
	struct SDL_Window* mHandle = nullptr;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;

	std::unique_ptr<Swapchain> mSwapchain = nullptr;

    bool mShouldResize = false;
    bool mShouldClose = false;
    bool mIsMinimized = false;
    bool mLockedCursor = false;
};
