#pragma once

#include "compute_effects_manager.h"
#include "pipeline_resource_manager.h"
#include "vk_types.h"
#include "deletion_queue.h"
#include "vk_descriptors.h"
#include <memory>
#include "asset_manager.h"

#include "volk.h"
#include "entt.hpp"

#include "vk_window.h"
#include "scene_manager.h"

#include <thread>

static constexpr unsigned int MAX_FRAMES_IN_FLIGHT = 3;
constexpr uint32_t MAX_IMAGE_WIDTH = 3840;
constexpr uint32_t MAX_IMAGE_HEIGHT = 2160;

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
		std::string entryPoint = "";
		bool vsync = false;
	};

	// initializes everything in the engine
	virtual void init(const std::vector<std::string>& cliArgs);
	virtual void parseCliArgs(const std::vector<std::string>& cliArgs);
	void parseConfig();
	void saveConfigToFile();

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

	PipelineResourceManager& getPipelineResourceManager() {
		return *mPipelineResourceManager;
	}

	AssetManager& getAssetManager() {
		return *mAssetManager;
	}

	ScriptManager& getScriptManager() {
	    return *mScriptManager;
	}

	SceneManager& getSceneManager() {
	    return *mSceneManager;
	}

	Window& getWindow() {
	    return *mWindow;
	}

	ComputeEffectsManager& getComputeEffectsManager() {
	    return *mComputeEffectsManager;
	}

	ImGuiContext& getImGuiContext() {
	    return mImGuiContext;
	}

	std::string getEntryPoint() {
	    return mEngineConfig.entryPoint;
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

	glm::vec2 getDrawExtent() {
	    return glm::vec2(mDrawExtent.width, mDrawExtent.height);
	}

	AllocatedImage& getShadowMap() {
	    return mShadowDepthImage;
	}

	ComputeEffect::Context getComputeContext(AllocatedImage* colorImage = nullptr);

	void immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function, bool waitResult = false);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);
	void *getDataFromBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false, bool createMipViews = false, bool msaa = false);
	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped = false);
	AllocatedImage createCubemap(VkExtent2D size, VkFormat format, VkImageUsageFlags usage, std::optional<glm::vec3> color = std::nullopt, bool mipMapped = false);

	void destroyImage(const AllocatedImage& image);
	void destroySampler(VkSampler sampler);

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
	void initVulkan();
	void initVMA();
	void initSwapchain();
	void initImages();
	void initCommands();
	void initSyncStructs();
	virtual void initImGui();

	// draw loop
	virtual void draw() = 0;
	void drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer commandBuffer);
	glm::mat4 renderShadowMap(VkCommandBuffer commandBuffer, uint32_t cascadeIndex, glm::mat4& lightView, glm::vec2 globalZ);

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
	AllocatedImage mBufferImage;

	AllocatedImage mMsaaDrawImage;

	const uint32_t SHADOW_CASCADES_COUNT = 4;
	const uint32_t SHADOW_DEPTH_IMAGE_WIDTH = 2048;
	const uint32_t SHADOW_DEPTH_IMAGE_HEIGHT = 2048;
	AllocatedImage mShadowDepthImage;

	// immediat submit structures
	VkFence mImmFence;
	VkCommandBuffer mImmCommandBuffer;
	VkCommandPool mImmCommandPool;

	std::filesystem::path mProjectPath = ".";

	std::unique_ptr<PipelineResourceManager> mPipelineResourceManager;
	std::unique_ptr<AssetManager> mAssetManager;
	std::unique_ptr<ComputeEffectsManager> mComputeEffectsManager;
	std::unique_ptr<SceneManager> mSceneManager;
	std::unique_ptr<ScriptManager> mScriptManager;

	RenderContext mRenderContext;
	ImGuiContext mImGuiContext;

	Stats mStats{};

	VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties{};
	float mMaxSamplerAnisotropy;

	float mShadowDepth = 1100.f;
};
