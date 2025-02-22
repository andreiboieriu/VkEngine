#pragma once

#include <list>
#include <string_view>
#include "volk.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vk_swapchain.h"
#include <set>

enum class InputState : uint8_t {
    IDLE,
    PRESSED,
    HELD,
    RELEASED
};

struct MouseInput {
    glm::vec<2, double> deltaPosition; // amount mouse moved in the last frame
    glm::vec<2, double> position; // current mouse position
    double deltaWheel; // amount mouse wheel moved in the last frame
    std::unordered_map<int, InputState> buttonStates;
};

struct Input {
    std::unordered_map<int, InputState> keyStates;
    MouseInput mouse;
};

class Window {

public:
    Window(std::string_view name, VkExtent2D extent);
    ~Window();

    bool shouldClose() {
        return glfwWindowShouldClose(mHandle);
    }

    // TODO: this
    bool isMinimized() {
        return false;
    }

    std::vector<const char*> getGLFWInstanceExtensions();
    void initImGuiGLFW();

    VkExtent2D getExtent() {
        return mExtent;
    }

    float getAspectRatio() {
        return (float)mExtent.width / (float)mExtent.height;
    }

    void createSwapchain(VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR);

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

    void pollEvents();

    void drawGui();

    // void toggleLockedCursor();

    bool isCursorLocked() {
        return mLockedCursor;
    }

    const Input& getInput() {
        return mInput;
    }

    void resize(uint32_t width, uint32_t height);

private:

    // caches newly pressed/released keys to update their state in the next frame
    // without iterating through the states unordered_map
    struct InputCache {
        std::list<int> pressedKeys;
        std::list<int> releasedKeys;
        std::list<int> pressedMouseButtons;
        std::list<int> releasedMouseButtons;
    };

    void init();
    void initInput();
    void initGLFW();
    void setWindowHints();
    void selectMonitor();
    void createSurface();

    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    void setCallbacks();

    void updateInput();

    void updateExtent();

    const std::string mName;
    VkExtent2D mExtent;

	// glfw window handle
	GLFWwindow *mHandle = nullptr;
    GLFWmonitor *mMonitor = nullptr;
    const GLFWvidmode* mVideoMode = nullptr;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    std::vector<VkPresentModeKHR> mAvailablePresentModes;
    int mSelectedPresentMode = 0;

	std::unique_ptr<Swapchain> mSwapchain = nullptr;

	Input mInput{};
    InputCache mInputCache;

    bool mShouldResize = false;
    bool mShouldClose = false;
    bool mIsMinimized = false;
    bool mLockedCursor = false;
};
