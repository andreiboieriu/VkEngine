#pragma once

#include <string_view>

#include <SDL.h>
#include "glm/ext/vector_int2.hpp"
#include "volk.h"

#include "vk_swapchain.h"
#include <set>

struct MouseInput {
    glm::ivec2 motion;
    glm::ivec2 position;
    float deltaWheel;
    std::set<Uint8> pressedButtons;
    std::set<Uint8> releasedButtons;
    Uint32 state;
};

struct Input {
    std::set<SDL_Keycode> pressedKeys;
    std::set<SDL_Keycode> releasedKeys;
    Uint8 keyboardState[SDL_NUM_SCANCODES];
    MouseInput mouse;
};

class Window {

public:
    Window(std::string_view name, VkExtent2D extent, SDL_WindowFlags windowFlags);
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

    void processSDLEvents();

    void toggleLockedCursor();

    void update();

    struct SDL_Window* getHandle() {
        return mHandle;
    }

    bool isCursorLocked() {
        return mLockedCursor;
    }

    const Input& getInput() {
        return mInput;
    }

private:
    void init(SDL_WindowFlags windowFlags);
    void handleWindowEvent(SDL_WindowEventID event);

    void updateExtent();
    void resize();

    const std::string mName;
    VkExtent2D mExtent;

	// sdl window handle
	struct SDL_Window* mHandle = nullptr;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;

	std::unique_ptr<Swapchain> mSwapchain = nullptr;
	Input mInput;

    bool mShouldResize = false;
    bool mShouldClose = false;
    bool mIsMinimized = false;
    bool mLockedCursor = false;
};
