// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include "vk_descriptors.h"
#include "deletion_queue.h"
#include "vk_loader.h"

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanEngine {
public:
	static VulkanEngine& get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//run main loop
	void run();

	FrameData& getCurrentFrame() {
		return mFrames[mFrameNumber % MAX_FRAMES_IN_FLIGHT];
	}

	void immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:

	void initVulkan();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initImGui();

	// TODO: delete later
	void initTrianglePipeline();

	void initMeshPipeline();

	void initDefaultData();


	void createSwapchain(uint32_t width, uint32_t height);
	void destroySwapchain();

	//draw loop
	void draw();
	void drawBackground(VkCommandBuffer commandBuffer);
	void drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer commandBuffer);

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

	VmaAllocator mAllocator;

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

	DeletionQueue mMainDeletionQueue;

	// draw resources
	AllocatedImage mDrawImage;
	VkExtent2D mDrawExtent;

	// descriptor resources
	DescriptorAllocator mDescriptorAllocator;
	VkDescriptorSet mDrawImageDescriptors;
	VkDescriptorSetLayout mDrawImageDescriptorLayout;

	// pipeline resources
	VkPipeline mGradientPipeline;
	VkPipelineLayout mGradientPipelineLayout;

	VkPipelineLayout mTrianglePipelineLayout;
	VkPipeline mTrianglePipeline;

	VkPipelineLayout mMeshPipelineLayout;
	VkPipeline mMeshPipeline;
	GPUMeshBuffers mRectangle;

	std::vector<std::shared_ptr<MeshAsset>> mTestMeshes;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect = 0;
	
	// immediat submit structures
	VkFence mImmFence;
	VkCommandBuffer mImmCommandBuffer;
	VkCommandPool mImmCommandPool;
};
