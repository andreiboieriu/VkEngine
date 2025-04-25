#include "vk_window.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <fmt/core.h>
#include <vector>
#include "vk_engine.h"
#include "vk_enum_string_helper.h"
#include "vk_types.h"
#include <imgui_impl_glfw.h>

Window::Window(
    std::string_view name,
    VkExtent2D extent,
    VulkanEngine& vkEngine
) : mName(name), mExtent(extent), mVkEngine(vkEngine) {
    init();
};

Window::~Window() {
    mSwapchain = nullptr;

    if (mSurface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(mVkEngine.getInstance(), mSurface, nullptr);
    }

    glfwDestroyWindow(mHandle);
    glfwTerminate();
}

void Window::initGLFW() {
    if (!glfwInit()) {
        fmt::println("Failed to initialize GLFW");
        exit(EXIT_FAILURE);
    }

    if (!glfwVulkanSupported()) {
        fmt::println("GLFW didn't find any vulkan support");
        exit(EXIT_FAILURE);
    }
}

void Window::setWindowHints() {
    // disable opengl context creation
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_FALSE);

    // set video mode hints

    // glfwWindowHint(GLFW_RED_BITS, mVideoMode->redBits);
    // glfwWindowHint(GLFW_GREEN_BITS, mVideoMode->greenBits);
    // glfwWindowHint(GLFW_BLUE_BITS, mVideoMode->blueBits);
    // glfwWindowHint(GLFW_REFRESH_RATE, mVideoMode->refreshRate);
}

void Window::selectMonitor() {
    mMonitor = glfwGetPrimaryMonitor();

    mVideoMode = glfwGetVideoMode(mMonitor);

    fmt::println("width: {}, height: {}, refresh: {}", mVideoMode->width, mVideoMode->height, mVideoMode->refreshRate);
}

void Window::initInput() {
    for (int16_t i = GLFW_KEY_UNKNOWN; i <= GLFW_KEY_LAST; i++) {
        mInput.keyStates[i] = InputState::IDLE;
    }

    for (uint8_t i = GLFW_MOUSE_BUTTON_1; i <= GLFW_MOUSE_BUTTON_LAST; i++) {
        mInput.mouse.buttonStates[i] = InputState::IDLE;
    }
}

void Window::init() {
    initGLFW();
    initInput();
    selectMonitor();
    setWindowHints();

    // create window
    mHandle = glfwCreateWindow(
        mExtent.width,
        mExtent.height,
        mName.c_str(),
        nullptr,
        nullptr
    );

    // set window user pointer to allow non-static member functions to be set as callbacks
    glfwSetWindowUserPointer(mHandle, this);

    setCallbacks();

    // enable raw mouse motion when cursor is locked for better input accuracy
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(mHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

void Window::createSwapchain(VkPresentModeKHR presentMode) {
    // get available present modes
    if (mAvailablePresentModes.empty()) {
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
        mVkEngine.getPhysicalDevice(),
        mSurface,
        &presentModeCount,
        nullptr);

        mAvailablePresentModes.resize(presentModeCount);

        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
            mVkEngine.getPhysicalDevice(),
            mSurface,
            &presentModeCount,
            mAvailablePresentModes.data()
        ));

        fmt::println("Available present modes:");

        for (auto& presentMode : mAvailablePresentModes) {
            fmt::println("{}", string_VkPresentModeKHR(presentMode));
        }
    }

    auto it = std::find(mAvailablePresentModes.begin(), mAvailablePresentModes.end(), presentMode);

    if (it == mAvailablePresentModes.end()) {
        fmt::println("Unsupported present mode, defaulting to fifo");
        presentMode = VK_PRESENT_MODE_FIFO_KHR;
        mSelectedPresentMode = std::find(
            mAvailablePresentModes.begin(),
            mAvailablePresentModes.end(),
            presentMode
        ) - mAvailablePresentModes.begin();
    } else {
        mSelectedPresentMode = it - mAvailablePresentModes.begin();
    }

    mSwapchain = std::make_unique<Swapchain>(mExtent, presentMode, mSurface, mVkEngine);
}

void Window::createSurface() {
    // create surface
    VK_CHECK(glfwCreateWindowSurface(
        mVkEngine.getInstance(),
        mHandle,
        nullptr,
        &mSurface
    ));
}

VkSurfaceKHR Window::getSurface() {
    if (mSurface == VK_NULL_HANDLE) {
        createSurface();
    }

    return mSurface;
}

std::vector<const char*> Window::getGLFWInstanceExtensions() {
    uint32_t count;
    const char** ppExtensions = glfwGetRequiredInstanceExtensions(&count);

    std::vector<const char*> extensions(count);

    for (uint32_t i = 0; i < count; i++) {
        extensions[i] = ppExtensions[i];
    }

    return extensions;
}

void Window::initImGuiGLFW() {
    ImGui_ImplGlfw_InitForVulkan(mHandle, true);
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    switch (action) {
        case GLFW_PRESS:
            mInput.keyStates[scancode] = InputState::PRESSED;
            mInputCache.pressedKeys.push_front(scancode);
            break;

        case GLFW_RELEASE:
            mInput.keyStates[scancode] = InputState::RELEASED;
            mInputCache.releasedKeys.push_front(scancode);
            break;
    }
}

void Window::cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    mInput.mouse.deltaPosition = {xpos - mInput.mouse.position.x, ypos - mInput.mouse.position.y};
    mInput.mouse.position = {xpos, ypos};
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    switch (action) {
        case GLFW_PRESS:
            mInput.mouse.buttonStates[button] = InputState::PRESSED;
            mInputCache.pressedMouseButtons.push_front(button);
            break;

        case GLFW_RELEASE:
            mInput.mouse.buttonStates[button] = InputState::RELEASED;
            mInputCache.releasedMouseButtons.push_front(button);
            break;
    }
}

void Window::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    mInput.mouse.deltaWheel = yoffset;
}

void Window::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    resize(width, height);
}

void Window::setCallbacks() {
    glfwSetKeyCallback(
        mHandle,
        [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->keyCallback(
                window,
                key,
                scancode,
                action,
                mods
            );
        }
    );

    glfwSetCursorPosCallback(
        mHandle,
        [](GLFWwindow* window, double xpos, double ypos) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->cursorPosCallback(
                window,
                xpos,
                ypos
            );
        }
    );

    glfwSetMouseButtonCallback(
        mHandle,
        [](GLFWwindow* window, int button, int action, int mods) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->mouseButtonCallback(
                window,
                button,
                action,
                mods
            );
        }
    );

    glfwSetScrollCallback(
        mHandle,
        [](GLFWwindow* window, double xoffset, double yoffset) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->scrollCallback(
                window,
                xoffset,
                yoffset
            );
        }
    );

    glfwSetFramebufferSizeCallback(
        mHandle,
        [](GLFWwindow* window, int width, int height) {
            static_cast<Window*>(glfwGetWindowUserPointer(window))->framebufferSizeCallback(
                window,
                width,
                height
            );
        }
    );
}

void Window::updateInput() {
    // update the newly pressed keys's state to HELD
    for (const int key : mInputCache.pressedKeys) {
        mInput.keyStates[key] = InputState::HELD;
    }

    // update the newly released keys's state to IDLE
    for (const int key : mInputCache.releasedKeys) {
        mInput.keyStates[key] = InputState::IDLE;
    }

    // update the newly pressed mouse buttons's state to HELD
    for (const int button : mInputCache.pressedKeys) {
        mInput.mouse.buttonStates[button] = InputState::HELD;
    }

    // update the newly pressed mouse buttons's state to IDLE
    for (const int button : mInputCache.releasedKeys) {
        mInput.mouse.buttonStates[button] = InputState::IDLE;
    }

    // reset input cache
    mInputCache = {};

    // reset mouse deltas
    mInput.mouse.deltaPosition = {0, 0};
    mInput.mouse.deltaWheel = 0.f;
}

void Window::pollEvents() {
    updateInput();
    glfwPollEvents();
}

void Window::resize(uint32_t width, uint32_t height) {
    // wait for gpu to finish tasks
    vkDeviceWaitIdle(mVkEngine.getDevice());

    // update extent
    mExtent = {width, height};

    // recreate swapchain
    mSwapchain = nullptr;
    mSwapchain = std::make_unique<Swapchain>(mExtent, mAvailablePresentModes[mSelectedPresentMode], mSurface, mVkEngine);
}

VkImage Window::getNextSwapchainImage(VkSemaphore& semaphore) {
    VkImage image = mSwapchain->getNextImage(semaphore);

    if (image == VK_NULL_HANDLE) {
        resize(mExtent.width, mExtent.height);
    }

    return image;
}

void Window::presentSwapchainImage(VkQueue graphicsQueue, VkSemaphore waitSemaphore) {
    if (!mSwapchain->presentImage(graphicsQueue, waitSemaphore)) {
        resize(mExtent.width, mExtent.height);
    }
}

void Window::drawGui() {
    if (ImGui::Begin("Window Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::SliderInt(
            "Present Mode",
            &mSelectedPresentMode,
            0,
            mAvailablePresentModes.size() - 1,
            string_VkPresentModeKHR(mAvailablePresentModes[mSelectedPresentMode]))) {
            resize(mExtent.width, mExtent.height);
        }

        ImGui::End();
    }
}

// void Window::toggleLockedCursor() {
//     mLockedCursor = !mLockedCursor;

//     if (mLockedCursor) {
//         SDL_SetRelativeMouseMode(SDL_TRUE);
//         SDL_ShowCursor(SDL_DISABLE);
//     } else {
//         SDL_SetRelativeMouseMode(SDL_FALSE);
//         SDL_ShowCursor(SDL_ENABLE);
//     }
// }
