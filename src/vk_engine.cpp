//> includes
#include "vk_engine.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vk_enum_string_helper.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#define VK_NO_PROTOTYPES
#define VOLK_IMPLEMENTATION
#include "volk.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_DEBUG_LOG
#include "vk_mem_alloc.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <SDL_vulkan.h>
#include "SDL_video.h"
#include "fmt/core.h"
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "vk_materials.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"

#include "VkBootstrap.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "stb_image.h"
#include <chrono>
#include <thread>

#include "ecs_systems/systems.h"

VulkanEngine* gLoadedEngine = nullptr;

entt::registry gRegistry;

VulkanEngine& VulkanEngine::get() { 
    return *gLoadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(gLoadedEngine == nullptr);
    gLoadedEngine = this;

    // create window
    mWindow = std::make_unique<Window>("Vulkan Engine", VkExtent2D{1600, 900}, (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE));

    mMainDeletionQueue.push([&]() {
        mWindow = nullptr;
    });

    // init vulkan
    initVulkan();
    initVMA();
    initSwapchain();
    initCommands();
    initSyncStructs();
    initDefaultData();
    initImGui();
    initECS();

    loadLoadingScreenData();

    // render loading screen
    std::thread t{&VulkanEngine::drawLoadingScreen, this};

    // load all other resources
    mFxaaEffect = std::make_unique<Fxaa>();

    mMainDeletionQueue.push([&]() {
        mFxaaEffect = nullptr;
    });

    mMaterialManager = std::make_unique<MaterialManager>();

    mMainDeletionQueue.push([&]() {
        mMaterialManager = nullptr;
    });

    mDescriptorManager = std::make_unique<DescriptorManager>(mMaterialManager->getSceneDescriptorSetLayout(), mMaterialManager->getMaterialDescriptorSetLayout());
    mMainDeletionQueue.push([&]() {
        mDescriptorManager = nullptr;
    });

    mResourceManager = std::make_unique<ResourceManager>();
    mMainDeletionQueue.push([&]() {
        mResourceManager = nullptr;
    });

    mScene = std::make_shared<Scene3D>("testScene");

    mMainDeletionQueue.push([&]() {
        mScene = nullptr;
    });

    // everything went fine
    mIsInitialized = true;

    // stop loading screen
    t.join();
}

void VulkanEngine::loadLoadingScreenData() {
    // load quad mesh
    std::vector<Vertex> vertices = {
        Vertex{.position = glm::vec3(-0.5f, 0.5f, 0.0f), .uvX = 0.f, .uvY = 1.f},
        Vertex{.position = glm::vec3(-0.5f, -0.5f, 0.0f), .uvX = 0.f, .uvY = 0.f},
        Vertex{.position = glm::vec3(0.5f, -0.5f, 0.0f), .uvX = 1.f, .uvY = 0.f},
        Vertex{.position = glm::vec3(0.5f, 0.5f, 0.0f), .uvX = 1.f, .uvY = 1.f},
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    mLoadingScreenData.meshBuffers = uploadMesh(indices, vertices);
    mLoadingScreenData.indexCount = indices.size();

    mMainDeletionQueue.push([&]() {
        destroyBuffer(mLoadingScreenData.meshBuffers.indexBuffer);
        destroyBuffer(mLoadingScreenData.meshBuffers.vertexBuffer);
    });

    // create descriptor set layout
    DescriptorLayoutBuilder builder;
    mLoadingScreenData.descriptorSetLayout = builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                                                    .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                                    .build(mDevice, 0, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    std::vector<VkDescriptorSetLayout> setLayouts = {mLoadingScreenData.descriptorSetLayout};
    std::vector<VkPushConstantRange> pushConstantRanges;

    // create pipeline layout
    mLoadingScreenData.pipelineLayout = vkutil::createPipelineLayout(
        setLayouts,
        pushConstantRanges
    );
    
    // create pipeline
    VkShaderModule vertShader;
    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/sprite.vert.spv", mDevice, &vertShader)) {
        fmt::println("failed to load sprite vertex shader");
    }

    if (!vkutil::loadShaderModule("shaders/sprite.frag.spv", mDevice, &fragShader)) {
        fmt::println("failed to load sprite frag shader");
    }

    PipelineBuilder pipelineBuilder;
    mLoadingScreenData.pipeline = pipelineBuilder.clear().setShaders(vertShader, fragShader)
                                              .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                              .setPolygonMode(VK_POLYGON_MODE_FILL)
                                              .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                                              .setMultisampling()
                                            //   .enableBlendingAdditive()
                                              .enableBlendingAlphaBlend()
                                              .disableDepthTesting()
                                            //   .enableDepthTesting(false, VK_COMPARE_OP_GREATER)
                                              .setColorAttachmentFormat(mDrawImage.imageFormat)
                                              .setDepthFormat(mDepthImage.imageFormat)
                                              .setLayout(mLoadingScreenData.pipelineLayout)
                                              .buildPipeline(mDevice,  0);

    vkDestroyShaderModule(mDevice, vertShader, nullptr);
    vkDestroyShaderModule(mDevice, fragShader, nullptr);

    mMainDeletionQueue.push([&]() {
        vkDestroyDescriptorSetLayout(mDevice, mLoadingScreenData.descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(mDevice, mLoadingScreenData.pipelineLayout, nullptr);
        vkDestroyPipeline(mDevice, mLoadingScreenData.pipeline, nullptr);
    });

    // load texture
    int width, height, channels;
    unsigned char* data = stbi_load("assets/loading.png", &width, &height , &channels, 4);
    fmt::println("channels: {}", channels);

    mLoadingScreenData.texture = createImage(data, VkExtent3D{(uint32_t)width, (uint32_t)height, 1}, VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_SAMPLED_BIT, false);

    stbi_image_free(data);

    mMainDeletionQueue.push([&]() {
        destroyImage(mLoadingScreenData.texture);
    });

    // create ubo buffer
    mLoadingScreenData.uboBuffer = createBuffer(sizeof(LoadingScreenData::Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    mMainDeletionQueue.push([&]() {
        destroyBuffer(mLoadingScreenData.uboBuffer);
    });

}

AllocatedBuffer VulkanEngine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memUsage) {
    // allocate buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;
    
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memUsage;
    allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(mAllocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.allocInfo));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        // get buffer device address
        VkBufferDeviceAddressInfo deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        deviceAddressInfo.buffer = newBuffer.buffer;

        newBuffer.deviceAddress = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);
    }

    return newBuffer;
}

void VulkanEngine::destroyBuffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(mAllocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newMesh;

    // create vertex buffer
    newMesh.vertexBuffer = createBuffer(
        vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
        );

    // get vertex buffer addess
    VkBufferDeviceAddressInfo deviceAddressInfo{};
    deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    deviceAddressInfo.buffer = newMesh.vertexBuffer.buffer;

    newMesh.vertexBufferAddress = vkGetBufferDeviceAddress(mDevice, &deviceAddressInfo);

    // create index buffer
    newMesh.indexBuffer = createBuffer(
        indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
        );

    // copy data to gpu using a staging buffer
    AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    // copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);

    // copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediateSubmit([&](VkCommandBuffer commandBuffer) {
        VkBufferCopy vertexCopy{};
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(commandBuffer, staging.buffer, newMesh.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{};
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(commandBuffer, staging.buffer, newMesh.indexBuffer.buffer, 1, &indexCopy);
    });

    // cleanup staging buffer
    destroyBuffer(staging);

    return newMesh;
}

// could be improved by running it on another queue
void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function) {
    // reset imd cmd fence and command buffer
    VK_CHECK(vkResetFences(mDevice, 1, &mImmFence));
    VK_CHECK(vkResetCommandBuffer(mImmCommandBuffer, 0));

    VkCommandBuffer commandBuffer = mImmCommandBuffer;

    // begin recording command buffer
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // call function passed as param
    function(commandBuffer);

    // finalize command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo commandInfo = vkinit::command_buffer_submit_info(commandBuffer);
    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandInfo, nullptr, nullptr);

    // submit command buffer to queue
    VK_CHECK(vkQueueSubmit2(mImmediateCommandsQueue, 1, &submitInfo, mImmFence));

    VK_CHECK(vkWaitForFences(mDevice, 1, &mImmFence, true, UINT64_MAX));
}

void VulkanEngine::initVulkan() {
    // init volk
    VK_CHECK(volkInitialize());

    // build vulkan instance using vkbootstrap
    vkb::InstanceBuilder builder{};

    unsigned int sdlExtCount;
    SDL_Vulkan_GetInstanceExtensions(mWindow->getHandle(), &sdlExtCount, NULL);

    std::vector<const char *> sdlExtensions(sdlExtCount);
    SDL_Vulkan_GetInstanceExtensions(mWindow->getHandle(), &sdlExtCount, sdlExtensions.data());

    vkb::Instance vkbInstance = builder.set_app_name("VkEngine")
        .request_validation_layers(mUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
        .enable_extensions(sdlExtensions)
        .build()
        .value();

    // get instance and messenger from vkbInstance
    mInstance = vkbInstance.instance;

    volkLoadInstance(mInstance);

    mDebugMessenger = vkbInstance.debug_messenger;

    // get needed vulkan features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures{};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = true;
    descriptorBufferFeatures.descriptorBufferCaptureReplay = true;
    descriptorBufferFeatures.descriptorBufferImageLayoutIgnored = true;
    descriptorBufferFeatures.descriptorBufferPushDescriptors = true;
    descriptorBufferFeatures.pNext = nullptr;

    VkPhysicalDeviceFeatures physicalDeviceFeatures{};
    physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;

    vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
    vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(mWindow->getSurface())
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
        .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
        .add_required_extension_features(descriptorBufferFeatures)
        .set_required_features(physicalDeviceFeatures)
        .select()
        .value();

    // create logical device using vkbootstrap
    std::vector<vkb::CustomQueueDescription> queueDescriptions;
    auto queueFamilies = vkbPhysicalDevice.get_queue_families();

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueDescriptions.push_back(vkb::CustomQueueDescription{
                i,
                std::vector<float> {
                    1.0f,
                    0.0f
                }
            });

            mGraphicsQueueFamily = i;
            mImmediateCommandsQueueFamily = i;

            break;
        }
    }

    vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = vkbDeviceBuilder.custom_queue_setup(queueDescriptions).build().value();

    // get device handles
    mDevice = vkbDevice.device;
    mPhysicalDevice = vkbPhysicalDevice.physical_device;

    volkLoadDevice(mDevice);

    // get queues
    vkGetDeviceQueue(mDevice, mGraphicsQueueFamily, 0, &mGraphicsQueue);
    vkGetDeviceQueue(mDevice, mImmediateCommandsQueueFamily, 1, &mImmediateCommandsQueue);

    // get descriptor buffer properties
    mDescriptorBufferProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2KHR deviceProperties{};
    deviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    deviceProperties.pNext = &mDescriptorBufferProperties;

    vkGetPhysicalDeviceProperties2KHR(mPhysicalDevice, &deviceProperties);

    mMaxSamplerAnisotropy = deviceProperties.properties.limits.maxSamplerAnisotropy;
}

void VulkanEngine::initVMA() {
    // init memory allocator
    VmaAllocatorCreateInfo vmaCreateInfo = vkinit::vmaCreateInfo(
        mPhysicalDevice,
        mDevice,
        mInstance,
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
        );

    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
    vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
    vulkanFunctions.vkFreeMemory = vkFreeMemory;
    vulkanFunctions.vkMapMemory = vkMapMemory;
    vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
    vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
    vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
    vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
    vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
    vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
    vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
    vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
    vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
    vulkanFunctions.vkCreateImage = vkCreateImage;
    vulkanFunctions.vkDestroyImage = vkDestroyImage;
    vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
    vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
    vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
    vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
    vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
    vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;

    vmaCreateInfo.pVulkanFunctions = &vulkanFunctions; 

    vmaCreateAllocator(&vmaCreateInfo, &mAllocator);

    mMainDeletionQueue.push([=, this]() {
        vmaDestroyAllocator(mAllocator);
    });
}

void VulkanEngine::initSwapchain() {
    mWindow->createSwapchain();

    // set draw image size to window size
    VkExtent3D drawImageExtent = {
        2560* 2,
        1440 * 2,
        1
    };

    mDrawImage.imageExtent = drawImageExtent;

    // set draw image format
    mDrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_STORAGE_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(mDrawImage.imageFormat, drawImageUsages, drawImageExtent);

    // allocate draw image from gpu local memory
    VmaAllocationCreateInfo imageAllocInfo{};
    imageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    imageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // create image
    vmaCreateImage(mAllocator, &imageCreateInfo, &imageAllocInfo, &mDrawImage.image, &mDrawImage.allocation, nullptr);

    // create image view
    VkImageViewCreateInfo imageViewInfo = vkinit::imageview_create_info(mDrawImage.imageFormat, mDrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(mDevice, &imageViewInfo, nullptr, &mDrawImage.imageView));

    // add image and image view to deletion queue
    mMainDeletionQueue.push([=, this]() {
        vkDestroyImageView(mDevice, mDrawImage.imageView, nullptr);
        vmaDestroyImage(mAllocator, mDrawImage.image, mDrawImage.allocation);
    });

    // initialize depth image
    mDepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;

    VkExtent3D depthImageExtent = drawImageExtent;

    mDepthImage.imageExtent = depthImageExtent;

    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo depthImageInfo = vkinit::image_create_info(mDepthImage.imageFormat, depthImageUsages, drawImageExtent);

    vmaCreateImage(mAllocator, &depthImageInfo, &imageAllocInfo, &mDepthImage.image, &mDepthImage.allocation, nullptr);

    VkImageViewCreateInfo depthViewInfo = vkinit::imageview_create_info(mDepthImage.imageFormat, mDepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(mDevice, &depthViewInfo, nullptr, &mDepthImage.imageView));

    mMainDeletionQueue.push([=, this]() {
        vkDestroyImageView(mDevice, mDepthImage.imageView, nullptr);
        vmaDestroyImage(mAllocator, mDepthImage.image, mDepthImage.allocation);
    });
}

void VulkanEngine::initCommands() {
    // create command pool for graphics queue commands
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
        mGraphicsQueueFamily,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        );

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mFrames[i].commandPool));

        // allocate command buffer from pool
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(
            mFrames[i].commandPool,
            1
            );

        VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdAllocInfo, &mFrames[i].mainCommandBuffer));
    }

    VkCommandPoolCreateInfo immCommandPoolInfo = vkinit::command_pool_create_info(
        mImmediateCommandsQueueFamily,
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        );

    // create command pool for immediate commands
    VK_CHECK(vkCreateCommandPool(mDevice, &immCommandPoolInfo, nullptr, &mImmCommandPool));

    // allocate command buffer for immediate commands
    VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(mImmCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &allocInfo, &mImmCommandBuffer));

    // add imm cmd resources to deletion queue
    mMainDeletionQueue.push([=, this]() {
        vkDestroyCommandPool(mDevice, mImmCommandPool, nullptr);
    });
}

void VulkanEngine::initSyncStructs() {
    // create synchronization structs
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mFrames[i].renderFence));

        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mFrames[i].swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &mFrames[i].renderSemaphore));
    }

    // create fence for imm commands
    VK_CHECK(vkCreateFence(mDevice, &fenceCreateInfo, nullptr, &mImmFence));

    // add imm cmd fence to deletion queue
    mMainDeletionQueue.push([=, this]() {
        vkDestroyFence(mDevice, mImmFence, nullptr);
    });
}

void VulkanEngine::initDefaultData() {
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	mWhiteImage = createImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	mGreyImage = createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	mBlackImage = createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 *16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	mErrorCheckerboardImage = createImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;
    sampl.anisotropyEnable = VK_TRUE;
    sampl.maxAnisotropy = mMaxSamplerAnisotropy;
	vkCreateSampler(mDevice, &sampl, nullptr, &mDefaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(mDevice, &sampl, nullptr, &mDefaultSamplerLinear);

	mMainDeletionQueue.push([=, this](){
		vkDestroySampler(mDevice,mDefaultSamplerNearest,nullptr);
		vkDestroySampler(mDevice,mDefaultSamplerLinear,nullptr);

		destroyImage(mWhiteImage);
		destroyImage(mGreyImage);
		destroyImage(mBlackImage);
		destroyImage(mErrorCheckerboardImage);
	});
}

void VulkanEngine::initImGui() {
    // create descriptor pool for Imgui
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiDescPool;
    VK_CHECK(vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &imguiDescPool));

    // initialize imgui library
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(mWindow->getHandle());

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = mInstance;
    initInfo.PhysicalDevice = mPhysicalDevice;
    initInfo.Device = mDevice;
    initInfo.Queue = mImmediateCommandsQueue;
    initInfo.DescriptorPool = imguiDescPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    VkFormat swapchainImageFormat = mWindow->getSwapchainImageFormat();

    // dynamic rendering parameters
    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add imgui resources to deletion queue
    mMainDeletionQueue.push([=, this]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(mDevice, imguiDescPool, nullptr);
    });
}

void VulkanEngine::initECS() {
    
}

void VulkanEngine::cleanup() {
    if (!mIsInitialized) {
        return;
    }

    // wait until gpu stops
    vkDeviceWaitIdle(mDevice);

    // free command pools
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(mDevice, mFrames[i].commandPool, nullptr);

        // destroy sync objects
        vkDestroyFence(mDevice, mFrames[i].renderFence, nullptr);
        vkDestroySemaphore(mDevice, mFrames[i].swapchainSemaphore, nullptr);
        vkDestroySemaphore(mDevice, mFrames[i].renderSemaphore, nullptr);

        mFrames[i].deletionQueue.flush();
    }

    mMainDeletionQueue.flush();

    vkDestroyDevice(mDevice, nullptr);

    vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
    vkDestroyInstance(mInstance, nullptr);

    // clear engine pointer
    gLoadedEngine = nullptr;
}

void VulkanEngine::drawLoadingScreen() {
    auto start = std::chrono::system_clock::now();

    while (!mIsInitialized) {
        // process SDL events
        UserInput userInput = mWindow->processSDLEvents();

        // end loop if window is closed
        if (mWindow->shouldClose()) {
            break;
        }

        // throttle speed if window is minimized
        if (mWindow->isMinimized()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        // wait until gpu finishes rendering last frame
        VK_CHECK(vkWaitForFences(mDevice, 1, &getCurrentFrame().renderFence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(mDevice, 1, &getCurrentFrame().renderFence));

        // flush deletion queue of current frame
        getCurrentFrame().deletionQueue.flush();

        VkImage swapchainImage = mWindow->getNextSwapchainImage(getCurrentFrame().swapchainSemaphore);

        if (swapchainImage == VK_NULL_HANDLE) {
            vkDestroySemaphore(mDevice, getCurrentFrame().swapchainSemaphore, nullptr);
            VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
            VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &getCurrentFrame().swapchainSemaphore));
        
            return;
        }

        VkExtent2D windowExtent = mWindow->getExtent();

        // set draw extent
        mDrawExtent.width = std::min(windowExtent.width, mDrawImage.imageExtent.width) * mEngineConfig.renderScale;
        mDrawExtent.height = std::min(windowExtent.height, mDrawImage.imageExtent.height) * mEngineConfig.renderScale;
        
        // get command buffer from current frame
        VkCommandBuffer commandBuffer = getCurrentFrame().mainCommandBuffer;

        // reset command buffer
        VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

        // begin recording command buffer
        VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

        vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        // drawGeometry(commandBuffer);

        VkClearValue clearValue{};
        clearValue.color = {1.0, 1.0, 0.0, 1.0};
        VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(mDrawImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(mDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo = vkinit::rendering_info(mDrawExtent, &colorAttachment, &depthAttachment);
        vkCmdBeginRendering(commandBuffer, &renderInfo);

        // set dynamic viewport and scissor
        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = mDrawExtent.height;
        viewport.width = mDrawExtent.width;
        viewport.height = -(float)(mDrawExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = mDrawExtent.width;
        scissor.extent.height = mDrawExtent.height;

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        start = end;

        float dt = elapsed.count() / 1000000.f;

        mLoadingScreenData.rotation -= 90 * dt;

        mLoadingScreenData.uboData.vertexBufferAddress = mLoadingScreenData.meshBuffers.vertexBufferAddress;

        mLoadingScreenData.uboData.modelMatrix = glm::translate(glm::mat4(1.f), glm::vec3(mDrawExtent.width / 2.f, mDrawExtent.height / 2.f, 1.f)) * glm::rotate(glm::mat4(1.f), glm::radians(mLoadingScreenData.rotation), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::scale(glm::mat4(1.f), glm::vec3(150.f, 150.f, 1.0f));

        mLoadingScreenData.uboData.projectionMatrix = glm::ortho(0.0f, (float)mDrawExtent.width, 0.0f, (float)mDrawExtent.height, -100.f, 100.f);
      
        *(LoadingScreenData::Ubo*)mLoadingScreenData.uboBuffer.allocInfo.pMappedData = mLoadingScreenData.uboData;

        // bind pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mLoadingScreenData.pipeline);

        // // push descriptors
        DescriptorWriter writer;
        std::vector<VkWriteDescriptorSet> writes = writer.writeBuffer(0, mLoadingScreenData.uboBuffer.buffer, sizeof(LoadingScreenData::Ubo), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                                         .writeImage(1, mLoadingScreenData.texture.imageView, mDefaultSamplerLinear, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                         .getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mLoadingScreenData.pipelineLayout, 0, (uint32_t)writes.size(), writes.data());

        vkCmdBindIndexBuffer(commandBuffer, mLoadingScreenData.meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(commandBuffer, mLoadingScreenData.indexCount, 1, 0, 0, 0);

        vkCmdEndRendering(commandBuffer);

        // transition draw image to general layout for adding post-processing
        // vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

        // fxaa(commandBuffer);
        // mFxaaEffect->execute(commandBuffer, mDrawImage, mDrawExtent);

        // transition draw and swapchain image into transfer layouts
        vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // copy draw image to swapchain image
        vkutil::copyImageToImage(commandBuffer, mDrawImage.image, swapchainImage, mDrawExtent, windowExtent);

        // transition swapchain image to Attachment Optimal so we can draw directly to it
        // vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // draw ImGui into the swapchain image
        // drawImGui(commandBuffer, mWindow->getCurrentSwapchainImageView());

        // transition swapchain image into a presentable layout
        vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        // finalize command buffer
        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        // prepare to submit command buffer to queue
        VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(commandBuffer);

        VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
        VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

        VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

        // submit command buffer to queue
        VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

        mWindow->presentSwapchainImage(mGraphicsQueue, getCurrentFrame().renderSemaphore);

        // increment frame number
        mFrameNumber++;
    }
}

void VulkanEngine::draw() {
    // get start time
    auto start = std::chrono::system_clock::now();

    // wait until gpu finishes rendering last frame
    VK_CHECK(vkWaitForFences(mDevice, 1, &getCurrentFrame().renderFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(mDevice, 1, &getCurrentFrame().renderFence));

    // flush deletion queue of current frame
    getCurrentFrame().deletionQueue.flush();

    VkImage swapchainImage = mWindow->getNextSwapchainImage(getCurrentFrame().swapchainSemaphore);

    if (swapchainImage == VK_NULL_HANDLE) {
        vkDestroySemaphore(mDevice, getCurrentFrame().swapchainSemaphore, nullptr);
        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &getCurrentFrame().swapchainSemaphore));
    
        return;
    }

    VkExtent2D windowExtent = mWindow->getExtent();

    // set draw extent
    mDrawExtent.width = std::min(windowExtent.width, mDrawImage.imageExtent.width) * mEngineConfig.renderScale;
    mDrawExtent.height = std::min(windowExtent.height, mDrawImage.imageExtent.height) * mEngineConfig.renderScale;
    

    // get command buffer from current frame
    VkCommandBuffer commandBuffer = getCurrentFrame().mainCommandBuffer;

    // reset command buffer
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // begin recording command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(commandBuffer);
    // drawSkybox(commandBuffer);

    // transition draw image to general layout for adding post-processing
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    // fxaa(commandBuffer);
    mFxaaEffect->execute(commandBuffer, mDrawImage, mDrawExtent);

    // transition draw and swapchain image into transfer layouts
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy draw image to swapchain image
    vkutil::copyImageToImage(commandBuffer, mDrawImage.image, swapchainImage, mDrawExtent, windowExtent);

    // transition swapchain image to Attachment Optimal so we can draw directly to it
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // draw ImGui into the swapchain image
    drawImGui(commandBuffer, mWindow->getCurrentSwapchainImageView());

    // transition swapchain image into a presentable layout
    vkutil::transitionImage(commandBuffer, swapchainImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // prepare to submit command buffer to queue
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(commandBuffer);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

    // submit command buffer to queue
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

    mWindow->presentSwapchainImage(mGraphicsQueue, getCurrentFrame().renderSemaphore);

    // increment frame number
    mFrameNumber++;

    // get end time
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.drawTimeBuffer += elapsed.count() / 1000.f;
}

void VulkanEngine::drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(mWindow->getExtent(), &colorAttachment, nullptr);

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    vkCmdEndRendering(commandBuffer);
}

void VulkanEngine::run() {
    SDL_Event e;

    auto currentTime = std::chrono::system_clock::now();

    // main loop
    while (true) {
        // get start time
        auto start = std::chrono::system_clock::now();

        // process SDL events
        UserInput userInput = mWindow->processSDLEvents();

        // end loop if window is closed
        if (mWindow->shouldClose()) {
            break;
        }

        // throttle speed if window is minimized
        if (mWindow->isMinimized()) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            continue;
        }

        drawGui();

        // get delta time
        auto newTime = std::chrono::system_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::microseconds>(newTime - currentTime).count() / 1000000.f;
        currentTime = newTime;

        updateScene(dt, userInput);

        draw();

        // get frame end time
        auto end = std::chrono::system_clock::now();

        // calculate elapsed time
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // update stats
        mStats.frameTimeBuffer += elapsed.count() / 1000.f; // convert to microseconds

        mStats.frameCount++;
        mStats.msElapsed += elapsed.count() / 1000.f;

        if (mStats.msElapsed >= 1000.f) {
            mStats.frameTime = mStats.frameTimeBuffer / mStats.frameCount;
            mStats.updateTime = mStats.updateTimeBuffer / mStats.frameCount;
            mStats.drawGeometryTime = mStats.drawGeometryTimeBuffer / mStats.frameCount;
            mStats.drawTime = mStats.drawTimeBuffer / mStats.frameCount;

            mStats.frameTimeBuffer = 0;
            mStats.updateTimeBuffer = 0;
            mStats.drawGeometryTimeBuffer = 0;
            mStats.drawTimeBuffer = 0;

            mStats.fps = mStats.frameCount;

            mStats.msElapsed -= 1000.f;
            mStats.frameCount = 0;
        }
    }
}

void VulkanEngine::drawGui() {
    // imgui new frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Engine Config", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Checkbox("Enable frustum culling", &mEngineConfig.enableFrustumCulling);
        ImGui::Checkbox("Enable draw sorting", &mEngineConfig.enableDrawSorting);
        ImGui::SliderFloat("Render Scale", &mEngineConfig.renderScale, 0.3f, 2.f);

        if (ImGui::TreeNode("FXAA")) {
            mFxaaEffect->drawGui();
            ImGui::TreePop();
        }
    }

    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("fps %i", mStats.fps);
        ImGui::Text("frametime %f ms", mStats.frameTime);
        ImGui::Text("update time %f ms", mStats.updateTime);
        ImGui::Text("draw time %f ms", mStats.drawTime);
        ImGui::Text("draw geometry time %f ms", mStats.drawGeometryTime);
        ImGui::Text("triangles %i", mStats.triangleCount);
        ImGui::Text("draws %i", mStats.drawCallCount);
    }

    ImGui::End();

    mScene->drawGui();

    // render ImGui
    ImGui::Render();
}

bool isVisible(const RenderObject& obj, const glm::mat4& viewProj) {
    std::array<glm::vec3, 8> corners {
        glm::vec3 { 1, 1, 1 },
        glm::vec3 { 1, 1, -1 },
        glm::vec3 { 1, -1, 1 },
        glm::vec3 { 1, -1, -1 },
        glm::vec3 { -1, 1, 1 },
        glm::vec3 { -1, 1, -1 },
        glm::vec3 { -1, -1, 1 },
        glm::vec3 { -1, -1, -1 },
    };

    glm::mat4 matrix = viewProj * obj.transform;

    glm::vec3 min = { 1.5, 1.5, 1.5 };
    glm::vec3 max = { -1.5, -1.5, -1.5 };

    for (int c = 0; c < 8; c++) {
        // project each corner into clip space
        glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

        // perspective correction
        v.x = v.x / v.w;
        v.y = v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3 { v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3 { v.x, v.y, v.z }, max);
    }

    // check the clip space box is within the view
    if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
        return false;
    } else {
        return true;
    }
}

void VulkanEngine::drawGeometry(VkCommandBuffer commandBuffer) {
    // reset stat counters
    mStats.drawCallCount = 0;
    mStats.triangleCount = 0;

    // get start time
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaqueObjectIndices;
    opaqueObjectIndices.reserve(mMainRenderContext.opaqueObjects.size());

    for (uint32_t i = 0; i < mMainRenderContext.opaqueObjects.size(); i++) {
        opaqueObjectIndices.push_back(i);
    }

    // sort opaque objects by material and mesh
    if (mEngineConfig.enableDrawSorting) {
        std::sort(opaqueObjectIndices.begin(), opaqueObjectIndices.end(), [&](const auto& iA, const auto& iB) {
            const RenderObject& A = mMainRenderContext.opaqueObjects[iA];
            const RenderObject& B = mMainRenderContext.opaqueObjects[iB];

            if (A.material == B.material) {
                return A.indexBuffer < B.indexBuffer;
            } else {
                return A.material < B.material;
            }
        });
    }

    std::vector<uint32_t> transparentObjectIndices;
    transparentObjectIndices.reserve(mMainRenderContext.transparentObjects.size());

    for (uint32_t i = 0; i < mMainRenderContext.transparentObjects.size(); i++) {
        transparentObjectIndices.push_back(i);
    }

    // sort transparent objects by material and mesh
    if (mEngineConfig.enableDrawSorting) {
        std::sort(transparentObjectIndices.begin(), transparentObjectIndices.end(), [&](const auto& iA, const auto& iB) {
            const RenderObject& A = mMainRenderContext.transparentObjects[iA];
            const RenderObject& B = mMainRenderContext.transparentObjects[iB];

            if (A.material == B.material) {
                return A.indexBuffer < B.indexBuffer;
            } else {
                return A.material < B.material;
            }
        });
    }

    VkClearValue clearValue{};
    clearValue.color = {1.0, 1.0, 0.0, 1.0};
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(mDrawImage.imageView, &clearValue, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(mDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(mDrawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(commandBuffer, &renderInfo);

    // set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = mDrawExtent.height;
    viewport.width = mDrawExtent.width;
    viewport.height = -(float)(mDrawExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = mDrawExtent.width;
    scissor.extent.height = mDrawExtent.height;

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    mDescriptorManager->bindDescriptorBuffers(commandBuffer);

    // track state
    MaterialPipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterialInstance = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;


    uint32_t bufferIndex = 0;
    VkDeviceSize bufferOffset = 0;

    auto draw = [&](const RenderObject& object) {
        if (object.material != lastMaterialInstance) {
            lastMaterialInstance = object.material;

            if (object.material->pipeline != lastPipeline) {
                lastPipeline = object.material->pipeline;

                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->pipeline);

                mScene->setGlobalDescriptorOffset(commandBuffer, object.material->pipeline->layout);
            }

            vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->layout, 1, 1, &bufferIndex, &object.material->descriptorOffset);
        }

        if (object.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = object.indexBuffer;

            vkCmdBindIndexBuffer(commandBuffer, object.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }

        GPUDrawPushConstants pushConstants;
        pushConstants.vertexBuffer = object.vertexBufferAddress;
        pushConstants.worldMatrix = object.transform;

        vkCmdPushConstants(commandBuffer, object.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, object.indexCount, 1, object.firstIndex, 0, 0);
    
        // update stats
        mStats.drawCallCount++;
        mStats.triangleCount += object.indexCount / 3;
    };

    for (auto& index : opaqueObjectIndices) {
        if (mEngineConfig.enableFrustumCulling && !isVisible(mMainRenderContext.opaqueObjects[index], mScene->getViewProj()))
            continue;

        draw(mMainRenderContext.opaqueObjects[index]);
    }

    // draw transparent objects after opaque ones
    for (auto& index : transparentObjectIndices) {
        if (mEngineConfig.enableFrustumCulling && !isVisible(mMainRenderContext.transparentObjects[index], mScene->getViewProj()))
            continue;

        draw(mMainRenderContext.transparentObjects[index]);
    }

    mScene->drawSkybox(commandBuffer);

    vkCmdEndRendering(commandBuffer);

    // get end time
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.drawGeometryTimeBuffer += elapsed.count() / 1000.f;
}

AllocatedImage VulkanEngine::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped) {
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imageInfo = vkinit::image_create_info(format, usage, size);

    if (mipMapped) {
        imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    // allocate image on dedicated gpu memory
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(mAllocator, &imageInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

    // change aspect flag is format is depth format
    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    // create image view
    VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, newImage.image, aspectFlags);
    viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;

    VK_CHECK(vkCreateImageView(mDevice, &viewInfo, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage VulkanEngine::createSkybox(void* data[6], VkExtent2D size, VkFormat format, VkImageUsageFlags usage) {
    size_t dataSize = size.width * size.height * 4;

    AllocatedBuffer uploadBuffer = createBuffer(dataSize * 6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // copy data to staging buffer
    for (int i = 0; i < 6; i++) {
        memcpy((char *)uploadBuffer.allocInfo.pMappedData + dataSize * i, data[i], dataSize);
    }

    // create skybox image
    AllocatedImage skybox{};
    skybox.imageFormat = format;
    skybox.imageExtent = {size.width, size.height, 1};

    VkImageCreateInfo skyboxInfo{};
    skyboxInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    skyboxInfo.imageType = VK_IMAGE_TYPE_2D;
    skyboxInfo.format = format;
    skyboxInfo.mipLevels = 1;
    skyboxInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    skyboxInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    skyboxInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    skyboxInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    skyboxInfo.extent = {size.width, size.height, 1};
    skyboxInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    skyboxInfo.arrayLayers = 6;
    skyboxInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(mAllocator, &skyboxInfo, &allocInfo, &skybox.image, &skybox.allocation, nullptr));

    // copy data to skybox image
    immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, skybox.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        uint32_t offset = 0;

        for (uint32_t face = 0; face < 6; face++) {
            VkBufferImageCopy bufferCopy{};
            bufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopy.imageSubresource.mipLevel = 0;
            bufferCopy.imageSubresource.baseArrayLayer = face;
            bufferCopy.imageSubresource.layerCount = 1;

            bufferCopy.imageExtent = {size.width, size.height, 1};
            bufferCopy.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopy);

            offset += dataSize;
        }

        vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, skybox.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

        vkutil::transitionImage(commandBuffer, skybox.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroyBuffer(uploadBuffer);

    // create skybox image view
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewInfo.format = format;

    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 6;
    imageViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageViewInfo.image = skybox.image;

    VK_CHECK(vkCreateImageView(mDevice, &imageViewInfo, nullptr, &skybox.imageView));

    return skybox;
}


// hardcoded to rgba 8 bit
AllocatedImage VulkanEngine::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped) {
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // copy data to staging buffer
    memcpy(uploadBuffer.allocInfo.pMappedData, data, dataSize);

    AllocatedImage newImage = createImage(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipMapped);

    immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = size.depth;
        copyRegion.imageExtent = size;

        vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipMapped) {
            vkutil::generateMipmaps(commandBuffer, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
        } else {
            vkutil::transitionImage(commandBuffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroyBuffer(uploadBuffer);

    return newImage;
}

void VulkanEngine::destroyImage(const AllocatedImage& image) {
    vkDestroyImageView(mDevice, image.imageView, nullptr);
    vmaDestroyImage(mAllocator, image.image, image.allocation);
}

void VulkanEngine::updateScene(float dt, const UserInput& userInput) {
    // get start time
    auto start = std::chrono::system_clock::now();

    mMainRenderContext.opaqueObjects.clear();
    mMainRenderContext.transparentObjects.clear();

    mScene->update(dt, mWindow->getAspectRatio(), userInput);
    // mScene->draw(mMainRenderContext);

    velocitySystem(dt);
    renderSystem(mMainRenderContext);


    // get end time
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.updateTimeBuffer += elapsed.count() / 1000.f;
}
