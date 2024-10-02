#include "vk_window.h"

#include <SDL_events.h>
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#include <SDL_vulkan.h>
#include <imgui_impl_sdl2.h>
#include <vulkan/vulkan_core.h>
#include "vk_engine.h"

Window::Window(std::string_view name, VkExtent2D extent, SDL_WindowFlags windowFlags) : mName(name), mExtent(extent) {
    init(windowFlags);
};

Window::~Window() {
    mSwapchain = nullptr;

    SDL_DestroyWindow(mHandle);

    if (mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(VulkanEngine::get().getInstance(), mSurface, nullptr);
    }
}

void Window::init(SDL_WindowFlags windowFlags) {
    SDL_Init(SDL_INIT_VIDEO);

    int displays = SDL_GetNumVideoDisplays();

    if (displays == 1) {
        mHandle = SDL_CreateWindow(
        mName.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        mExtent.width,
        mExtent.height,
        windowFlags
        );

        return;
    }

    std::vector<SDL_Rect> displayBounds;
    for (int i = 0; i < displays; i++) {
        displayBounds.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displayBounds.back());
    }

    mHandle = SDL_CreateWindow(
        mName.c_str(),
        SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        SDL_WINDOWPOS_CENTERED_DISPLAY(1),
        mExtent.width,
        mExtent.height,
        windowFlags
    );
}

void Window::createSwapchain() {
    // mSwapchain = std::make_unique<Swapchain>(mExtent, VK_PRESENT_MODE_FIFO_KHR, mSurface);
    mSwapchain = std::make_unique<Swapchain>(mExtent, VK_PRESENT_MODE_MAILBOX_KHR, mSurface);
}

VkSurfaceKHR Window::getSurface() {
    if (mSurface == VK_NULL_HANDLE) {
        SDL_Vulkan_CreateSurface(mHandle, VulkanEngine::get().getInstance(), &mSurface);
    }

    return mSurface;
}

void Window::processSDLEvents() {
    SDL_Event e;

    // clear previous input
    mInput = Input{};

    // Handle events on queue
    while (SDL_PollEvent(&e) != 0) {
        switch (e.type) {
        // close the window when user alt-f4s or clicks the X button
        case SDL_QUIT:
            mShouldClose = true;
            return;

        case SDL_WINDOWEVENT:
            handleWindowEvent((SDL_WindowEventID)e.window.event);
            break;

        case SDL_KEYDOWN:
            mInput.pressedKeys.insert(e.key.keysym.sym);
            break;

        case SDL_KEYUP:
            mInput.releasedKeys.insert(e.key.keysym.sym);
            break;

        case SDL_MOUSEMOTION:
            mInput.mouse.motion.x = e.motion.xrel;
            mInput.mouse.motion.y = e.motion.yrel;
            break;

        case SDL_MOUSEBUTTONUP:
            mInput.mouse.releasedButtons.insert(e.button.button);
            break;

        case SDL_MOUSEBUTTONDOWN:
            mInput.mouse.pressedButtons.insert(e.button.button);
            break;

        case SDL_MOUSEWHEEL:
            mInput.mouse.deltaWheel = e.wheel.preciseY;
            break;

        default:
            break;
        }

        // send SDL event to imgui
        ImGui_ImplSDL2_ProcessEvent(&e);
    }

    // copy keyboard state
    memcpy(mInput.keyboardState, SDL_GetKeyboardState(nullptr), 512 * sizeof(Uint8));

    // get mouse state
    mInput.mouse.state = SDL_GetMouseState(&mInput.mouse.position.x, &mInput.mouse.position.y);
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
