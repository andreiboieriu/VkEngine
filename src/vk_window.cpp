#include "vk_window.h"

#include <SDL_vulkan.h>
#include "imgui_impl_sdl2.h"
#include "vk_engine.h"

Window::Window(std::string_view name, VkExtent2D extent, SDL_WindowFlags windowFlags, uint32_t monitorIdx) : mName(name), mExtent(extent) {
    init(windowFlags, monitorIdx);
};

Window::~Window() {
    mSwapchain = nullptr;

    SDL_DestroyWindow(mHandle);

    if (mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(VulkanEngine::get().getInstance(), mSurface, nullptr);
    }
}

void Window::init(SDL_WindowFlags windowFlags, uint32_t monitorIdx) {
    SDL_Init(SDL_INIT_VIDEO);

    int displays = SDL_GetNumVideoDisplays();
    assert(displays > monitorIdx);

    std::vector<SDL_Rect> displayBounds;
    for (int i = 0; i < displays; i++) {
        displayBounds.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displayBounds.back());
    }

    mHandle = SDL_CreateWindow(
        mName.c_str(),
        displayBounds[monitorIdx].x,
        displayBounds[monitorIdx].y,
        mExtent.width,
        mExtent.height,
        windowFlags
    );
}

void Window::createSwapchain() {
    mSwapchain = std::make_unique<Swapchain>(mExtent, VK_PRESENT_MODE_MAILBOX_KHR, mSurface);
}

VkSurfaceKHR Window::getSurface() {
    if (mSurface == VK_NULL_HANDLE) {
        SDL_Vulkan_CreateSurface(mHandle, VulkanEngine::get().getInstance(), &mSurface);
    }

    return mSurface;
}

UserInput Window::processSDLEvents() {
    SDL_Event e;

    UserInput userInput{};

    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
        // close the window when user alt-f4s or clicks the X button
        case SDL_QUIT:
            mShouldClose = true;
            return userInput;

        case SDL_WINDOWEVENT:
            handleWindowEvent((SDL_WindowEventID)e.window.event);
            break;

        case SDL_KEYDOWN:
            userInput.pressedKeys.insert(e.key.keysym.sym);
            break;

        case SDL_KEYUP:
            userInput.releasedKeys.insert(e.key.keysym.sym);
            break;

        case SDL_MOUSEMOTION:
            userInput.mouseXRel = e.motion.xrel;
            userInput.mouseYRel = e.motion.yrel;
            break;

        case SDL_MOUSEWHEEL:
            userInput.mouseWheel = e.wheel.preciseY;
            break;

        default:
            break;
        }

        // send SDL event to imgui
        ImGui_ImplSDL2_ProcessEvent(&e);
    }

    // toggle locked cursor
    if (userInput.pressedKeys.contains(SDLK_ESCAPE)) {
        toggleLockedCursor();
    }

    userInput.GuiMode = !mLockedCursor;

    return userInput;
}

void Window::handleWindowEvent(SDL_WindowEventID event) {
    switch (event) {
    case SDL_WINDOWEVENT_MINIMIZED:
        mIsMinimized = true;
        break;

    case SDL_WINDOWEVENT_RESTORED:
        mIsMinimized = false;
        break;

    case SDL_WINDOWEVENT_RESIZED:
        mShouldResize = true;
        break;

    default:
        break;
    }
}

void Window::updateExtent() {
    int w, h;
    SDL_GetWindowSize(mHandle, &w, &h);

    // get surface capabilities to correct window size (SDL bug)
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanEngine::get().getPhysicalDevice(), mSurface, &surfaceCapabilities));

    mExtent.width = SDL_clamp(w, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
    mExtent.height = SDL_clamp(h, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
}

void Window::resize() {
    // wait for gpu to finish tasks
    vkDeviceWaitIdle(VulkanEngine::get().getDevice());

    updateExtent();

    // recreate swapchain
    mSwapchain = nullptr;
    mSwapchain = std::make_unique<Swapchain>(mExtent, VK_PRESENT_MODE_FIFO_KHR, mSurface);
}

VkImage Window::getNextSwapchainImage(VkSemaphore& semaphore) {
    VkImage image = mSwapchain->getNextImage(semaphore);

    if (image == VK_NULL_HANDLE) {
        resize();
    }

    return image;
}

void Window::presentSwapchainImage(VkQueue graphicsQueue, VkSemaphore waitSemaphore) {
    if (!mSwapchain->presentImage(graphicsQueue, waitSemaphore)) {
        resize();
    }
}

void Window::toggleLockedCursor() {
    mLockedCursor = !mLockedCursor;

    if (mLockedCursor) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
    } else {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }
}
