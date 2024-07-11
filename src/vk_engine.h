// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer; 
};

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanEngine {
public:
	static VulkanEngine& get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	FrameData& getCurrentFrame() {
		return mFrames[mFrameNumber % MAX_FRAMES_IN_FLIGHT];
	}

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();

	// Vulkan library handle
	VkInstance mInstance;

	// Vulkan debug output handle
	VkDebugUtilsMessengerEXT mDebugMessenger;

	// chosen GPU
	VkPhysicalDevice mPhysicalDevice;

	// Vulkan logical device
	VkDevice mDevice;

	// Vulkan window surface
	VkSurfaceKHR mSurface;

	// swapchain related
	VkSwapchainKHR mSwapchain;
	VkFormat mSwapchainImageFormat;

	std::vector<VkImage> mSwapchainImages;
	std::vector<VkImageView> mSwapchainImageViews;
	VkExtent2D mSwapchainExtent;

	// flags
	bool mIsInitialized = false;
	bool mStopRendering = false;
	bool mUseValidationLayers = true;

	int mFrameNumber = 0;

	FrameData mFrames[MAX_FRAMES_IN_FLIGHT];
	VkQueue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

	// window size basically
	VkExtent2D mWindowExtent = {1600, 900};

	// sdl window handle
	struct SDL_Window* mWindow = nullptr;
};
