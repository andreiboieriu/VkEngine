#define IMGUI_IMPL_VULKAN_USE_VOLK
#include "vk_engine.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"

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
#include <fmt/core.h>
#include "vk_descriptors.h"
#include "vk_initializers.h"
#include "pipeline_resource_manager.h"
#include "vk_types.h"
#include "vk_images.h"

#include "VkBootstrap.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "stb_image.h"
#include <chrono>
#include <thread>
#include "vk_scene.h"

void VulkanEngine::parseCliArgs(const std::vector<std::string>& cliArgs) {
    mUseValidationLayers = false;

    for (auto& arg : cliArgs) {
        if (arg == "--debug") {
            mUseValidationLayers = true;
            fmt::println("Enabled validation layers");
        }
    }
}

void VulkanEngine::parseConfig() {
    std::ifstream file(mProjectPath.string() + "/engine_config.json");
    if (!file) {
        fmt::println("Could not open engine config file.\n");
        return;
    }

    nlohmann::json j;
    file >> j;

    mEngineConfig.entryPoint.resize(100);

    if (j.contains("entryPoint") && j["entryPoint"].is_string()) {
        mEngineConfig.entryPoint = j["entryPoint"];
    } else {
        fmt::println("Engine config error: entryPoint field missing or not a string.\n");
    }

    if (j.contains("frustumCulling") && j["frustumCulling"].is_boolean()) {
        mEngineConfig.enableFrustumCulling = j["frustumCulling"];
    }

    if (j.contains("drawSorting") && j["drawSorting"].is_boolean()) {
        mEngineConfig.enableDrawSorting = j["drawSorting"];
    }

    if (j.contains("vsync") && j["vsync"].is_boolean()) {
        mEngineConfig.vsync = j["vsync"];
    }

    if (j.contains("renderScale") && j["renderScale"].is_number()) {
        mEngineConfig.renderScale = j["renderScale"];
    }
}

void VulkanEngine::init(const std::vector<std::string>& cliArgs)
{
    // parse cli args
    parseCliArgs(cliArgs);
    parseConfig();

    // create window
    mWindow = std::make_unique<Window>("Vulkan Engine", VkExtent2D{2560, 1440}, *this);

    mMainDeletionQueue.push([&]() {
        mWindow = nullptr;
    });

    // init vulkan
    initVulkan();
    initVMA();
    initSwapchain();
    initCommands();
    initSyncStructs();
    initImages();

    mAssetManager = std::make_unique<AssetManager>(*this);
    mPipelineResourceManager = std::make_unique<PipelineResourceManager>(*this);
    mComputeEffectsManager = std::make_unique<ComputeEffectsManager>(*this);
    mComputeEffectsManager->loadEffects();

    mAssetManager->loadBrdfLut();

    mSceneManager = std::make_unique<SceneManager>(*this);
    mScriptManager = std::make_unique<ScriptManager>(*this);
    mSceneManager->init();

    mMainDeletionQueue.push([&]() {
        mSceneManager = nullptr;
        mPipelineResourceManager = nullptr;
        mAssetManager = nullptr;
        mComputeEffectsManager = nullptr;
        mScriptManager = nullptr;
    });
}

ComputeEffect::Context VulkanEngine::getComputeContext(AllocatedImage* colorImage) {
    VkSampler linear = mAssetManager ? *mAssetManager->getSampler("linear") : VK_NULL_HANDLE;

    return ComputeEffect::Context{
        .colorImage = colorImage == nullptr ? mDrawImage : *colorImage,
        .depthImage = mDepthImage,
        .bufferImage = mBufferImage,
        .screenSize = mDrawExtent,
        .linearSampler = linear,
        .additionalImages = mAssetManager->getComputeRelatedImages(),
        .sunScreenPos = mSceneManager->getActiveScene().getSunScreenPos()
    };
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

void VulkanEngine::destroySampler(VkSampler sampler) {
    vkDestroySampler(mDevice, sampler, nullptr);
}

void VulkanEngine::destroyBuffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(mAllocator, buffer.buffer, buffer.allocation);
}

void *VulkanEngine::getDataFromBuffer(const AllocatedBuffer& buffer) {
    return buffer.allocation->GetMappedData();
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

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer commandBuffer)>&& function, bool waitResult) {
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

    // TODO: maybe make this more efficient
    if (waitResult) {
        vkQueueWaitIdle(mImmediateCommandsQueue);
    }
}

void VulkanEngine::initVulkan() {
    // init volk
    VK_CHECK(volkInitialize());

    // build vulkan instance using vkbootstrap
    vkb::InstanceBuilder builder{};

    vkb::Instance vkbInstance = builder.set_app_name("VkEngine")
        // .request_validation_layers(mUseValidationLayers)
        .enable_validation_layers(true)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .enable_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)
        .enable_extension(VK_KHR_DISPLAY_EXTENSION_NAME) // enable display info
        .enable_extension(VK_KHR_SURFACE_EXTENSION_NAME)
        .enable_extensions(mWindow->getGLFWInstanceExtensions())
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

    vkbSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(mWindow->getSurface())
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)
        .add_required_extension(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME)
        .add_required_extension_features(descriptorBufferFeatures)
        .set_required_features(physicalDeviceFeatures);

    auto devices = vkbSelector.select_devices(vkb::DeviceSelectionMode::only_fully_suitable).value();
    vkb::PhysicalDevice vkbPhysicalDevice;

    for (auto dev : devices) {
        fmt::println("found device: {}", dev.name);

        if (dev.name.starts_with("NVIDIA")) {
            vkbPhysicalDevice = dev;
        }
    }

    fmt::println("Physical Device name: {}", vkbPhysicalDevice.name);

    // create logical device using vkbootstrap
    std::vector<vkb::CustomQueueDescription> queueDescriptions;
    auto queueFamilies = vkbPhysicalDevice.get_queue_families();

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queueDescriptions.push_back(vkb::CustomQueueDescription{
                i,
                std::vector<float> {
                    1.0f,
                    0.8f
                }
            });

            mGraphicsQueueFamily = i;
            mImmediateCommandsQueueFamily = i;

            break;
        }
    }

    // for (uint32_t i = 1; i < queueFamilies.size(); i++) {
    //     if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
    //         std::cout << "Transfer queue count: " << queueFamilies[i].queueCount << "\n";
    //         queueDescriptions.push_back(vkb::CustomQueueDescription{
    //             i,
    //             std::vector<float> {
    //                 1.0f
    //             }
    //         });

    //         mImmediateCommandsQueueFamily = i;

    //         break;
    //     }
    // }

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
    mWindow->createSwapchain(mEngineConfig.vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR);
}

void VulkanEngine::initImages() {
    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_STORAGE_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;

    mDrawImage = createImage(
        VkExtent3D{MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT, 1},
        VK_FORMAT_R16G16B16A16_SFLOAT,
        drawImageUsages
    );

    mDepthImage = createImage(
        VkExtent3D{MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT, 1},
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    );

    mBufferImage = createImage(
        VkExtent3D{MAX_IMAGE_WIDTH, MAX_IMAGE_HEIGHT, 1},
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        true,
        true
    );

    mShadowDepthImage = createImage(
        VkExtent3D{SHADOW_DEPTH_IMAGE_WIDTH * SHADOW_CASCADES_COUNT, SHADOW_DEPTH_IMAGE_HEIGHT, 1},
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );

    immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, mBufferImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    });

    mMainDeletionQueue.push([&, this]() {
        destroyImage(mDrawImage);
        destroyImage(mDepthImage);
        destroyImage(mBufferImage);
        destroyImage(mShadowDepthImage);
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
    mWindow->initImGuiGLFW();

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = mInstance;
    initInfo.PhysicalDevice = mPhysicalDevice;
    initInfo.Device = mDevice;
    initInfo.Queue = mGraphicsQueue;
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
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(mDevice, imguiDescPool, nullptr);
    });
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
}

void VulkanEngine::drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(mWindow->getExtent(), &colorAttachment, nullptr);

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    vkCmdEndRendering(commandBuffer);
}

void VulkanEngine::run() {
    auto currentTime = std::chrono::system_clock::now();

    // main loop
    while (true) {
        // get start time
        auto start = std::chrono::system_clock::now();

        // poll GLFW events
        mWindow->pollEvents();

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
        mDeltaTime = std::chrono::duration_cast<std::chrono::microseconds>(newTime - currentTime).count() / 1000000.f;
        currentTime = newTime;

        update();

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
            mStats.postEffectsTime = mStats.postEffectsTimeBuffer / mStats.frameCount;

            mStats.frameTimeBuffer = 0;
            mStats.updateTimeBuffer = 0;
            mStats.drawGeometryTimeBuffer = 0;
            mStats.drawTimeBuffer = 0;
            mStats.postEffectsTimeBuffer = 0;

            mStats.fps = mStats.frameCount;

            mStats.msElapsed -= 1000.f;
            mStats.frameCount = 0;
        }
    }

    vkDeviceWaitIdle(mDevice);
}

bool isVisible(const GLTFRenderObject& obj, const glm::mat4& viewProj) {
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

    glm::vec3 min = { 1.2, 1.2, 1.2 };
    glm::vec3 max = { -1.2, -1.2, -1.2 };

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

glm::mat4 VulkanEngine::renderShadowMap(VkCommandBuffer commandBuffer, uint32_t cascadeIndex, glm::mat4& lightView, glm::vec2 globalZ) {
    auto& planes = mSceneManager->getActiveScene().getSceneData().cascadePlanes;
    const std::array<float, 3> cascadePlanes = {planes[0], planes[1], planes[2]};

    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(mShadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    if (cascadeIndex != 0) {
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    }

    depthAttachment.clearValue.depthStencil.depth = 1.f;
    VkRenderingInfo renderInfo = vkinit::rendering_info(VkExtent2D{SHADOW_DEPTH_IMAGE_WIDTH * SHADOW_CASCADES_COUNT, SHADOW_DEPTH_IMAGE_HEIGHT}, nullptr, &depthAttachment);
    vkCmdBeginRendering(commandBuffer, &renderInfo);

    // set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = float(cascadeIndex * SHADOW_DEPTH_IMAGE_WIDTH);
    viewport.y = SHADOW_DEPTH_IMAGE_HEIGHT;
    viewport.width = SHADOW_DEPTH_IMAGE_WIDTH;
    viewport.height = -(float)(SHADOW_DEPTH_IMAGE_HEIGHT);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = cascadeIndex * SHADOW_DEPTH_IMAGE_WIDTH;
    scissor.offset.y = 0;
    scissor.extent.width = SHADOW_DEPTH_IMAGE_WIDTH;
    scissor.extent.height = SHADOW_DEPTH_IMAGE_HEIGHT;

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    float near = cascadeIndex == 0 ? mRenderContext.near : cascadePlanes[cascadeIndex - 1];
    float far = cascadeIndex == 3 ? mRenderContext.far : cascadePlanes[cascadeIndex];

    glm::mat4 projection = glm::perspective(glm::radians(mRenderContext.fov), getWindowAspectRatio(), near, far);

    glm::mat4 invVP = glm::inverse(projection * mRenderContext.sceneData.view);
    std::vector<glm::vec3> frustumCorners;
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) {
                glm::vec4 pt = invVP * glm::vec4(
                    2.0f * x - 1.0f,
                    2.0f * y - 1.0f,
                    (z == 0) ? 0.0 : 1.0,
                    1.0f);
                pt /= pt.w;
                frustumCorners.push_back(glm::vec3(pt));
            }
        }
    }

    glm::vec3 frustumCenter(0.0f);
    for (const auto& corner : frustumCorners)
        frustumCenter += corner;
    frustumCenter /= frustumCorners.size();

    float sphereRadius = 0.0f;
    for (const auto& corner : frustumCorners) {
        float distance = glm::length(corner - frustumCenter);
        sphereRadius = std::max(sphereRadius, distance);
    }

    glm::vec3 sphereCenterLightSpace = glm::vec3(lightView * glm::vec4(frustumCenter, 1.0));

    // --- Start of New/Corrected Logic ---

    // 1. Define the initial, un-snapped projection bounds from the sphere
    glm::vec3 minBounds = sphereCenterLightSpace - sphereRadius;
    glm::vec3 maxBounds = sphereCenterLightSpace + sphereRadius;

    // 2. Calculate the size of the orthographic box
    glm::vec3 boundsSize = maxBounds - minBounds;

    // 3. CORRECTED Texel Size Calculation
    //    We divide by the width of a SINGLE cascade, not the entire atlas.
    float texelSizeX = boundsSize.x / (SHADOW_DEPTH_IMAGE_WIDTH * SHADOW_CASCADES_COUNT);
    float texelSizeY = boundsSize.y / SHADOW_DEPTH_IMAGE_HEIGHT;

    // 4. Snap the projection's minimum corner to the nearest texel boundary
    //    This is done by dividing by the texel size, flooring the result, and multiplying back.
    minBounds.x = floor(minBounds.x / texelSizeX) * texelSizeX;
    minBounds.y = floor(minBounds.y / texelSizeY) * texelSizeY;

    // 5. Recalculate the maximum corner based on the snapped minimum corner.
    //    This ensures the projection box size remains constant, preventing shimmering.
    maxBounds.x = minBounds.x + boundsSize.x;
    maxBounds.y = minBounds.y + boundsSize.y;

    glm::mat4 lightProjection = glm::ortho(
        minBounds.x, maxBounds.x,
        minBounds.y, maxBounds.y,
        globalZ.x, globalZ.y
    );

    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    auto pipeline = mPipelineResourceManager->getPipeline(PipelineResourceManager::PipelineType::DIR_SHADOW_MAP);

    auto draw = [&](const GLTFRenderObject& object) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindIndexBuffer(commandBuffer, object.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        DirShadowPushConstants pushConstants;
        pushConstants.worldMatrix = lightSpaceMatrix * object.transform;
        pushConstants.vertexBuffer = object.vertexBufferAddress;

        vkCmdPushConstants(commandBuffer, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DirShadowPushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, object.indexCount, 1, object.firstIndex, 0, 0);
    };

    for (auto& object : mRenderContext.opaqueObjects) {
        draw(object);
    }

    vkCmdEndRendering(commandBuffer);

    return lightSpaceMatrix;
}

void VulkanEngine::drawGeometry(VkCommandBuffer commandBuffer) {
    // reset stat counters
    mStats.drawCallCount = 0;
    mStats.triangleCount = 0;

    // get start time
    auto start = std::chrono::system_clock::now();

    std::vector<uint32_t> opaqueObjectIndices;
    opaqueObjectIndices.reserve(mRenderContext.opaqueObjects.size());

    for (uint32_t i = 0; i < mRenderContext.opaqueObjects.size(); i++) {
        opaqueObjectIndices.push_back(i);
    }

    // sort opaque objects by material and mesh
    if (mEngineConfig.enableDrawSorting) {
        std::sort(opaqueObjectIndices.begin(), opaqueObjectIndices.end(), [&](const auto& iA, const auto& iB) {
            const GLTFRenderObject& A = mRenderContext.opaqueObjects[iA];
            const GLTFRenderObject& B = mRenderContext.opaqueObjects[iB];

            if (A.material == B.material) {
                return A.indexBuffer < B.indexBuffer;
            } else {
                return A.material < B.material;
            }
        });
    }

    std::vector<uint32_t> transparentObjectIndices;
    transparentObjectIndices.reserve(mRenderContext.transparentObjects.size());

    for (uint32_t i = 0; i < mRenderContext.transparentObjects.size(); i++) {
        transparentObjectIndices.push_back(i);
    }

    // sort transparent objects by material and mesh
    if (mEngineConfig.enableDrawSorting) {
        std::sort(transparentObjectIndices.begin(), transparentObjectIndices.end(), [&](const auto& iA, const auto& iB) {
            const GLTFRenderObject& A = mRenderContext.transparentObjects[iA];
            const GLTFRenderObject& B = mRenderContext.transparentObjects[iB];

            if (A.material == B.material) {
                return A.indexBuffer < B.indexBuffer;
            } else {
                return A.material < B.material;
            }
        });
    }

    glm::vec3 cameraPos = glm::vec3(mRenderContext.sceneData.viewPosition);
    glm::vec3 lightDir = glm::vec3(mRenderContext.sceneData.sunlightDirection);

    glm::mat4 stableLightView = glm::lookAt(
        cameraPos + lightDir * 500.0f, // Position the light's "camera" far behind the player
        cameraPos,                     // Make it look directly at the player
        glm::vec3(0.0f, 1.0f, 0.0f)    // Use the world's 'up' vector to keep it from rolling
    );

    float globalMinZ = -mShadowDepth;
    float globalMaxZ = mShadowDepth;

    float lightDepthRange = globalMaxZ - globalMinZ;
    mSceneManager->getActiveScene().getSceneData().shadowConfig.w = lightDepthRange;

    for (int c = 0; c < SHADOW_CASCADES_COUNT; ++c) {
        mSceneManager->getActiveScene().getSceneData().lightSpace[c] = renderShadowMap(commandBuffer, c, stableLightView, glm::vec2(globalMinZ, globalMaxZ));
    }

    mSceneManager->getActiveScene().updateUBO(mFrameNumber);

    VkClearValue clearValue{};
    glm::vec4 clearColor = mRenderContext.sceneData.clearColor;
    clearValue.color = {clearColor.x, clearColor.y, clearColor.z, 1.0};
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

    mPipelineResourceManager->bindDescriptorBuffers(commandBuffer);

    // track state
    Pipeline* lastPipeline = nullptr;
    MaterialInstance* lastMaterialInstance = nullptr;
    VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

    uint32_t bufferIndex = 0;
    VkDeviceSize bufferOffset = 0;

    auto draw = [&](const GLTFRenderObject& object) {
        if (object.material != lastMaterialInstance) {
            lastMaterialInstance = object.material;

            if (object.material->pipeline != lastPipeline) {
                lastPipeline = object.material->pipeline;

                vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->pipeline);

                mSceneManager->getActiveScene().setGlobalDescriptorOffset(commandBuffer, object.material->pipeline->layout, mFrameNumber);
            }

            vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline->layout, 1, 1, &bufferIndex, &object.material->descriptorOffset);
        }

        if (object.indexBuffer != lastIndexBuffer) {
            lastIndexBuffer = object.indexBuffer;

            vkCmdBindIndexBuffer(commandBuffer, object.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }

        PBRPushConstants pushConstants;
        pushConstants.vertexBuffer = object.vertexBufferAddress;
        pushConstants.worldMatrix = object.transform;

        vkCmdPushConstants(commandBuffer, object.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PBRPushConstants), &pushConstants);

        vkCmdDrawIndexed(commandBuffer, object.indexCount, 1, object.firstIndex, 0, 0);

        // update stats
        mStats.drawCallCount++;
        mStats.triangleCount += object.indexCount / 3;
    };

    for (auto& index : opaqueObjectIndices) {
        if (mEngineConfig.enableFrustumCulling && !isVisible(mRenderContext.opaqueObjects[index], mRenderContext.sceneData.viewProjection))
            continue;

        draw(mRenderContext.opaqueObjects[index]);
    }

    auto linearSampler = *mAssetManager->getSampler("linear");

    if (mRenderContext.skybox != nullptr) {
        auto pipeline = mPipelineResourceManager->getPipeline(PipelineResourceManager::PipelineType::SKYBOX);
        auto cubeMesh = mAssetManager->getMesh("cube");
        SkyboxPushConstants pushConstants{};
        pushConstants.projViewMatrix = mRenderContext.sceneData.projection * glm::mat4(glm::mat3(mRenderContext.sceneData.view));
        pushConstants.vertexBuffer = cubeMesh->meshBuffers.vertexBufferAddress;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
        vkCmdBindIndexBuffer(commandBuffer, cubeMesh->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

        DescriptorWriter writer;
        std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, mRenderContext.skybox->envMap.imageView, linearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                                   .getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

        vkCmdPushConstants(commandBuffer, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SkyboxPushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, cubeMesh->surfaces[0].count, 1, cubeMesh->surfaces[0].startIndex, 0, 0);
    }

    // draw transparent objects after opaque ones
    for (auto& index : transparentObjectIndices) {
        if (mEngineConfig.enableFrustumCulling && !isVisible(mRenderContext.transparentObjects[index], mRenderContext.sceneData.viewProjection))
            continue;

        draw(mRenderContext.transparentObjects[index]);
    }

    auto spritePipeline = mPipelineResourceManager->getPipeline(PipelineResourceManager::PipelineType::SPRITE);
    auto quadMesh = mAssetManager->getMesh("quad");
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline->pipeline);
    vkCmdBindIndexBuffer(commandBuffer, quadMesh->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    glm::mat4 orthoProjection = glm::ortho(0.0f, (float)mDrawExtent.width, 0.0f, (float)mDrawExtent.height, -100.f, 100.f);

    for (auto sprite : mRenderContext.sprites) {
        // push constants
        SpritePushConstants pushConstants;
        pushConstants.vertexBuffer = quadMesh->meshBuffers.vertexBufferAddress;
        pushConstants.worldMatrix = orthoProjection * sprite.transform;

        vkCmdPushConstants(commandBuffer, spritePipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstants), &pushConstants);

        // push descriptors
        DescriptorWriter writer;
        std::vector<VkWriteDescriptorSet> writes = writer.writeImage(0, sprite.image->imageView, linearSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                         .getWrites();

        vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spritePipeline->layout, 0, (uint32_t)writes.size(), writes.data());

        // draw quad
        vkCmdDrawIndexed(commandBuffer, quadMesh->surfaces[0].count, 1, quadMesh->surfaces[0].startIndex, 0, 0);

        // update stats
        mStats.drawCallCount++;
        mStats.triangleCount += quadMesh->surfaces[0].count / 3;
    }

    vkCmdEndRendering(commandBuffer);

    // get end time
    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    mStats.drawGeometryTimeBuffer += elapsed.count() / 1000.f;
}

AllocatedImage VulkanEngine::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped, bool createMipViews, bool msaa) {
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo imageInfo = vkinit::image_create_info(format, usage, size);

    if (msaa) {
        imageInfo.samples = VK_SAMPLE_COUNT_8_BIT;
    }

    if (mipMapped) {
        imageInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
        newImage.mipLevels = imageInfo.mipLevels;
    } else {
        newImage.mipLevels = 1;
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

    if (mipMapped && createMipViews) {
        newImage.mipViews.resize(newImage.mipLevels);

        for (uint32_t i = 0; i < newImage.mipLevels; i++) {
            VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(VK_FORMAT_R16G16B16A16_SFLOAT, newImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseMipLevel = i;

            VK_CHECK(vkCreateImageView(mDevice, &viewInfo, nullptr, &newImage.mipViews[i]));
        }
    }

    return newImage;
}

AllocatedImage VulkanEngine::createCubemap(
    VkExtent2D size,
    VkFormat format,
    VkImageUsageFlags usage,
    std::optional<glm::vec3> color,
    bool mipMapped
) {
    // create image
    AllocatedImage cubemap{};
    cubemap.imageFormat = format;
    cubemap.imageExtent = {size.width, size.height, 1};
    cubemap.mipLevels = 1;

    VkImageCreateInfo cubemapInfo{};
    cubemapInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    cubemapInfo.imageType = VK_IMAGE_TYPE_2D;
    cubemapInfo.format = format;
    cubemapInfo.mipLevels = 1;
    cubemapInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    cubemapInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    cubemapInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    cubemapInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cubemapInfo.extent = {size.width, size.height, 1};
    cubemapInfo.usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    cubemapInfo.arrayLayers = 6;
    cubemapInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (mipMapped) {
        cubemapInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
        cubemap.mipLevels = cubemapInfo.mipLevels;
    }

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(mAllocator, &cubemapInfo, &allocInfo, &cubemap.image, &cubemap.allocation, nullptr));

    // create cubemap image view
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewInfo.format = format;

    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 6;
    imageViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageViewInfo.image = cubemap.image;

    VK_CHECK(vkCreateImageView(mDevice, &imageViewInfo, nullptr, &cubemap.imageView));

    if (color == std::nullopt) {
        return cubemap;
    }

    // upload color data to cubemap
    uint32_t pixelSize = vkutil::getPixelSize(format);

    if (!pixelSize) {
        fmt::println("Error: attempting to create image with unsupported format {}", string_VkFormat(format));
    }

    size_t dataSize = 6 * size.width * size.height * pixelSize;
    AllocatedBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    uint32_t colorInt = glm::packUnorm4x8(glm::vec4(color.value(), 1.f));
    std::vector<uint32_t> colorData(dataSize / sizeof(uint32_t), colorInt);

    // copy data to staging buffer
    memcpy(uploadBuffer.allocInfo.pMappedData, colorData.data(), dataSize);

    immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, cubemap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 6;
        copyRegion.imageExtent = {size.width, size.height, 1};

        vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        if (mipMapped) {
            vkutil::generateMipmaps(commandBuffer, cubemap.image, VkExtent2D{cubemap.imageExtent.width, cubemap.imageExtent.height});
            cubemap.mipLevels = int(std::floor(std::log2(std::max(cubemap.imageExtent.width, cubemap.imageExtent.height)))) + 1;
        } else {
            vkutil::transitionImage(commandBuffer, cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroyBuffer(uploadBuffer);

    return cubemap;
}

// only for color images
AllocatedImage VulkanEngine::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipMapped) {
    uint32_t pixelSize = vkutil::getPixelSize(format);

    if (!pixelSize) {
        fmt::println("Error: attempting to create image with unsupported format {}", string_VkFormat(format));
    }

    size_t dataSize = size.depth * size.width * size.height * pixelSize;
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

    for (int i = 0; i < image.mipViews.size(); i++) {
        vkDestroyImageView(mDevice, image.mipViews[i], nullptr);
    }
}

void VulkanEngine::saveConfigToFile() {
    std::filesystem::path filePath = mProjectPath.string() + "/engine_config.json";
    fmt::println("Saving engine config to {}", filePath.c_str());

    std::ofstream outFile(filePath);

    nlohmann::json j;
    j["renderScale"] = mEngineConfig.renderScale;
    j["frustumCulling"] = mEngineConfig.enableFrustumCulling;
    j["drawSorting"] = mEngineConfig.enableDrawSorting;
    j["vsync"] = mEngineConfig.vsync;
    j["entryPoint"] = mEngineConfig.entryPoint;

    outFile << j.dump(2);

    outFile.close();
}
