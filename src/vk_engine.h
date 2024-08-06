#pragma once

#include "compute_effects/fxaa.h"
#include "vk_materials.h"
#include "vk_scene.h"
#include "vk_types.h"
#include "deletion_queue.h"
#include <memory>

#include "volk.h"
#include "entt.hpp"

#include "vk_window.h"

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

	struct FrameData {
		VkCommandPool commandPool;
		VkCommandBuffer mainCommandBuffer;

		VkSemaphore swapchainSemaphore;
		VkSemaphore renderSemaphore;
		VkFence renderFence;

		DeletionQueue deletionQueue;
	};

	struct FXAAConfig {

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

	const VkDevice& getDevice() {
		return mDevice;
	}

	const VkPhysicalDevice& getPhysicalDevice() {
		return mPhysicalDevice;
	}

	const VkInstance& getInstance() {
		return mInstance;
	}

	AllocatedImage& getErrorTexture() {
		return mErrorCheckerboardImage;
	}

	AllocatedImage& getWhiteTexture() {
		return mWhiteImage;
	}

	AllocatedImage& getBlackTexture() {
		return mBlackImage;
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

	VkPhysicalDeviceDescriptorBufferPropertiesEXT getDescriptorBufferProperties() {
		return mDescriptorBufferProperties;
	}

	VkFormat getDrawImageFormat() {
		return mDrawImage.imageFormat;
	}

	VkFormat getDepthImageFormat() {
		return mDepthImage.imageFormat;
	}

	void immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createSkybox(void* data[6], VkExtent2D size, VkFormat format, VkImageUsageFlags usage);
	void destroyImage(const AllocatedImage& image);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	void updateScene(float dt, const UserInput& userInput);

private:

	void initVulkan();
	void initVMA();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();
	void initPipelines();
	void initComputeEffects();
	void initImGui();

	void initECS();

	void initDefaultData();

	//draw loop
	void draw();
	void drawBackground(VkCommandBuffer commandBuffer);
	void drawSkybox(VkCommandBuffer commandBuffer);
	void drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer commandBuffer);
	void fxaa(VkCommandBuffer commandBuffer);

	// Vulkan library handle
	VkInstance mInstance = VK_NULL_HANDLE;

	// Vulkan debug output handle
	VkDebugUtilsMessengerEXT mDebugMessenger;

	// chosen GPU
	VkPhysicalDevice mPhysicalDevice;

	// Vulkan logical device
	VkDevice mDevice;

	std::unique_ptr<Window> mWindow;

	VmaAllocator mAllocator;

	// flags
	bool mIsInitialized = false;
	bool mUseValidationLayers = true;

	int mFrameNumber = 0;

	FrameData mFrames[MAX_FRAMES_IN_FLIGHT];
	VkQueue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

	bool mResizeRequested = false;

	DeletionQueue mMainDeletionQueue;

	// draw resources
	AllocatedImage mDrawImage;
	VkExtent2D mDrawExtent;
	float mRenderScale = 1.0f;

	AllocatedImage mDepthImage;

	// // descriptor resources
	// VkDescriptorSetLayout mComputeDescriptorLayout;

	// VkPipelineLayout mComputePipelineLayout;
	// std::vector<ComputeEffect> backgroundEffects;
	// int currentBackgroundEffect = 0;
	
	// immediat submit structures
	VkFence mImmFence;
	VkCommandBuffer mImmCommandBuffer;
	VkCommandPool mImmCommandPool;

	// texture test resources
	AllocatedImage mWhiteImage;
	AllocatedImage mBlackImage;
	AllocatedImage mGreyImage;
	AllocatedImage mErrorCheckerboardImage;

	VkSampler mDefaultSamplerLinear;
	VkSampler mDefaultSamplerNearest;

	GLTFMetallicRoughness mMetalRoughMaterial;

	RenderContext mMainRenderContext;

	std::shared_ptr<Scene3D> mScene;

	Stats mStats{};

	VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties{};

	bool mCursorLocked = true;

	std::unique_ptr<Fxaa> mFxaaEffect;
};
