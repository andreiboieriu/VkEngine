#pragma once

#include "compute_effects/fxaa.h"
#include "vk_materials.h"
#include "vk_scene.h"
#include "vk_types.h"
#include "deletion_queue.h"
#include "vk_descriptors.h"
#include <memory>
#include "resource_manager.h"

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

	struct LoadingScreenData {
		GPUMeshBuffers meshBuffers;
		uint32_t indexCount;

		VkPipelineLayout pipelineLayout;
		VkDescriptorSetLayout descriptorSetLayout;
		VkPipeline pipeline;

		AllocatedImage texture;
		AllocatedBuffer uboBuffer;

		struct Ubo {
			glm::mat4 projectionMatrix;
			glm::mat4 modelMatrix;
			VkDeviceAddress vertexBufferAddress;
			glm::vec2 padding0;
			glm::vec4 padding1;
			glm::vec4 padding2;
			glm::vec4 padding3;
			glm::mat4 padding4;
		};

		Ubo uboData;
		float rotation = 0.f;
	};

	struct EngineConfig {
		float renderScale = 1.0f;
		bool enableFrustumCulling = true;
		bool enableDrawSorting = true;
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

	MaterialManager& getMaterialManager() {
		return *mMaterialManager;
	}

	DescriptorManager& getDescriptorManager() {
		return *mDescriptorManager;
	}

	ResourceManager& getResourceManager() {
		return *mResourceManager;
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
	void initImGui();

	void initECS();

	void initDefaultData();

	void loadLoadingScreenData();

	//draw loop
	void draw();
	void drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer commandBuffer);

	void drawLoadingScreen();

	void drawGui();

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

	VkSampler mDefaultSamplerLinear;
	VkSampler mDefaultSamplerNearest;

	std::unique_ptr<DescriptorManager> mDescriptorManager;
	std::unique_ptr<MaterialManager> mMaterialManager;
	std::unique_ptr<ResourceManager> mResourceManager;

	RenderContext mMainRenderContext;

	std::shared_ptr<Scene3D> mScene;

	Stats mStats{};

	VkPhysicalDeviceDescriptorBufferPropertiesEXT mDescriptorBufferProperties{};

	bool mCursorLocked = true;

	std::unique_ptr<Fxaa> mFxaaEffect;

	float mMaxSamplerAnisotropy;

	LoadingScreenData mLoadingScreenData;
};
