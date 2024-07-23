#pragma once

#include "game_object.h"
#include "vk_materials.h"
#include "vk_scene.h"
#include "vk_types.h"
#include "vk_descriptors.h"
#include "deletion_queue.h"
#include "vk_loader.h"
#include <memory>
#include <string>
#include <unordered_map>
#include "camera.h"

#include "volk.h"
#include "entt.hpp"

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanEngine {
public:
	struct Stats {
		float frameTime;
		int triangleCount;
		int drawCallCount;
		float updateTime;
		float drawGeometryTime;
		float drawTime;

		float frameTimeBuffer;
		float updateTimeBuffer;
		float drawGeometryTimeBuffer;
		float drawTimeBuffer;

		float msElapsed;
		int frameCount;

		int fps;
	};

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

	VkDevice& getDevice() {
		return mDevice;
	}

	AllocatedImage& getErrorTexture() {
		return mErrorCheckerboardImage;
	}

	AllocatedImage& getWhiteTexture() {
		return mWhiteImage;
	}

	VkSampler& getDefaultLinearSampler() {
		return mDefaultSamplerLinear;
	}

	VkSampler& getDefaultNearestSampler() {
		return mDefaultSamplerNearest;
	}

	GLTFMetallicRoughness& getGLTFMRCreator() {
		return mMetalRoughMaterial;
	}

	void immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	void destroyImage(const AllocatedImage& image);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	void updateScene();

private:

	void initVulkan();
	void initVolk();
	void initVMA();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initImGui();

	void initECS();

	void initDefaultData();

	void createSwapchain(uint32_t width, uint32_t height);
	void resizeSwapchain();
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

	bool mResizeRequested = false;

	DeletionQueue mMainDeletionQueue;

	// draw resources
	AllocatedImage mDrawImage;
	VkExtent2D mDrawExtent;
	float mRenderScale = 1.0f;

	AllocatedImage mDepthImage;

	// descriptor resources
	VkDescriptorSet mDrawImageDescriptors;
	VkDescriptorSetLayout mDrawImageDescriptorLayout;

	VkPipelineLayout mGradientPipelineLayout;
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect = 0;
	
	// immediat submit structures
	VkFence mImmFence;
	VkCommandBuffer mImmCommandBuffer;
	VkCommandPool mImmCommandPool;

	// scene resources
	GPUSceneData mSceneData;

	// texture test resources
	AllocatedImage mWhiteImage;
	AllocatedImage mBlackImage;
	AllocatedImage mGreyImage;
	AllocatedImage mErrorCheckerboardImage;

	VkSampler mDefaultSamplerLinear;
	VkSampler mDefaultSamplerNearest;

	GLTFMetallicRoughness mMetalRoughMaterial;

	RenderContext mMainRenderContext;

	// TODO: move somewhere else
	Camera mCamera;

	// std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> mLoadedScenes;
	std::shared_ptr<LoadedGLTF> mTestGLTF;

	// GameObject mTestObject;
	std::shared_ptr<GameObject> mTestObject;

	// std::shared_ptr<System> mRenderSystem;

	Stats mStats{};
};
