// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"

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

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();

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

	// flags
	bool mIsInitialized = false;
	bool mStopRendering = false;
	bool mUseValidationLayers = true;

	int mFrameNumber = 0;

	// window size basically
	VkExtent2D mWindowExtent = {1600, 900};

	// sdl window handle
	struct SDL_Window* mWindow = nullptr;
};
