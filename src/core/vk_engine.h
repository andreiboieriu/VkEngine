#pragma once

#include "compute_effects/compute_effect.h"
#include "pipeline_resource_manager.h"
#include "vk_scene.h"
#include "vk_types.h"
#include "deletion_queue.h"
#include "vk_descriptors.h"
#include <memory>
#include "asset_manager.h"

#include "volk.h"
#include "entt.hpp"

#include "vk_window.h"

#include <thread>

constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 2;

class VulkanEngine {
public:
	struct Stats {
		float frameTime;
		int triangleCount;
		int drawCallCount;
		float updateTime;
		float drawGeometryTime;
		float postEffectsTime;
		float drawTime;

		float frameTimeBuffer;
		float updateTimeBuffer;
		float drawGeometryTimeBuffer;
		float postEffectsTimeBuffer;
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

	struct EngineConfig {
		float renderScale = 1.0f;
		bool enableFrustumCulling = true;
		bool enableDrawSorting = true;
	};

	// initializes everything in the engine
	void init(const std::vector<std::string>& cliArgs);

	// shuts down the engine
	void cleanup();

	// run main loop
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

	AllocatedImage& getDefaultNormalMap() {
		return mDefaultNormalMap;
	}

	VkSampler& getDefaultLinearSampler() {
		return mDefaultSamplerLinear;
	}

	VkSampler& getDefaultNearestSampler() {
		return mDefaultSamplerNearest;
	}

	PipelineResourceManager& getPipelineResourceManager() {
		return *mPipelineResourceManager;
	}

	AssetManager& getAssetManager() {
		return *mAssetManager;
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

	float getMaxSamplerAnisotropy() {
		return mMaxSamplerAnisotropy;
	}

	void immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function, bool waitResult = false);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createEmptyCubemap(VkExtent2D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);

	void destroyImage(const AllocatedImage& image);

	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	float getDeltaTime() {
	   return mDeltaTime;
	}

	const float& getDeltaTimeRef() {
	   return mDeltaTime;
	}

	const Input& getInput() {
	   return mWindow->getInput();
	}

	// TODO: see about this later
	float getWindowAspectRatio() {
	   return mWindow->getAspectRatio();
	}

protected:
    void parseCliArgs(const std::vector<std::string>& cliArgs);
	void initVulkan();
	void initVMA();
	void initSwapchain();
	void initCommands();
	void initSyncStructs();
	void initImGui();
	void initECS();
	void initDefaultData();

	// draw loop
	virtual void draw() = 0;
	void drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer commandBuffer);

	void drawLoadingScreen();

	virtual void drawGui() = 0;
	virtual void update() = 0;

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
	bool mUseValidationLayers = false;

	int mFrameNumber = 0;
	float mDeltaTime = 0.f;

	FrameData mFrames[MAX_FRAMES_IN_FLIGHT];
	VkQueue mGraphicsQueue;
	uint32_t mGraphicsQueueFamily;

	VkQueue mImmediateCommandsQueue;
	uint32_t mImmediateCommandsQueueFamily;

	EngineConfig mEngineConfig;

	DeletionQueue mMainDeletionQueue;

	// draw resources
	AllocatedImage mDrawImage;
	VkExtent2D mDrawExtent;
	AllocatedImage mDepthImage;

	// immediat submit structures
	VkFence mImmFence;
	VkCommandBuffer mImmCommandBuffer;
	VkCommandPool mImmCommandPool;

	// texture test resources
	AllocatedImage mWhiteImage;
	AllocatedImage mBlackImage;
	AllocatedImage mGreyImage;
	AllocatedImage mErrorCheckerboardImage;
	AllocatedImage mDefaultNormalMap;

	VkSampler mDefaultSamplerLinear;
	VkSampler mDefaultSamplerNearest;

	std::unique_ptr<PipelineResourceManager> mPipelineResourceManager;
	std::unique_ptr<AssetManager> mAssetManager;

	RenderContext mRenderContext;

	std::shared_ptr<Scene3D> mScene;
	std::shared_ptr<Scene3D> mLoadingScene;

	Stats mStats{};

	VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties{};

	std::unique_ptr<ComputeEffect> mFxaaEffect;
	std::unique_ptr<ComputeEffect> mToneMappingEffect;
	std::unique_ptr<ComputeEffect> mBloomEffect;

	float mMaxSamplerAnisotropy;
};
