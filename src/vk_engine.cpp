//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan_core.h>

#include "SDL_stdinc.h"
#include "SDL_video.h"
#include "fmt/core.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "vk_descriptors.h"
#include "vk_enum_string_helper.h"
#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"

#include "VkBootstrap.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include <chrono>
#include <thread>

VulkanEngine* gLoadedEngine = nullptr;

VulkanEngine& VulkanEngine::get() { 
    return *gLoadedEngine;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(gLoadedEngine == nullptr);
    gLoadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    mWindow = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        mWindowExtent.width,
        mWindowExtent.height,
        windowFlags);

    // initialize vulkan
    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructs();
    initDescriptors();
    initPipelines();
    initImGui();

    initDefaultData();

    // everything went fine
    mIsInitialized = true;
}

AllocatedBuffer VulkanEngine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage	memUsage) {
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
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, mImmFence));

    VK_CHECK(vkWaitForFences(mDevice, 1, &mImmFence, true, 9e9));
}

void VulkanEngine::initVulkan() {
    vkb::InstanceBuilder builder{};

    // build vulkan instance using vkbootstrap
    vkb::Instance vkbInstance = builder.set_app_name("VkEngine")
        .request_validation_layers(mUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build()
        .value();

    // get instance and messenger from vkbInstance
    mInstance = vkbInstance.instance;
    mDebugMessenger = vkbInstance.debug_messenger;

    // create vulkan surface
    SDL_Vulkan_CreateSurface(mWindow, mInstance, &mSurface);

    // get needed vulkan features
    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    // select gpu using vkbootstrap
    vkb::PhysicalDeviceSelector vkbSelector{vkbInstance};
    vkb::PhysicalDevice vkbPhysicalDevice = vkbSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(mSurface)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select()
        .value();

    // create logical device using vkbootstrap
    vkb::DeviceBuilder vkbDeviceBuilder{vkbPhysicalDevice};
    vkb::Device vkbDevice = vkbDeviceBuilder.build().value();

    // get device handles
    mDevice = vkbDevice.device;
    mPhysicalDevice = vkbPhysicalDevice.physical_device;

    // get graphics queue using vkb
    mGraphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    mGraphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // init memory allocator
    VmaAllocatorCreateInfo vmaCreateInfo = vkinit::vmaCreateInfo(
        mPhysicalDevice,
        mDevice,
        mInstance,
        VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT
        );

    vmaCreateAllocator(&vmaCreateInfo, &mAllocator);

    mMainDeletionQueue.push([&]() {
        vmaDestroyAllocator(mAllocator);
    });
}

void VulkanEngine::initSwapchain() {
    createSwapchain(mWindowExtent.width, mWindowExtent.height);

    // set draw image size to window size
    VkExtent3D drawImageExtent = {
        2560,
        1440,
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

    VkExtent3D depthImageExtent = {
        mWindowExtent.width,
        mWindowExtent.height,
        1
    };

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

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    // set swapchain image format
    mSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // build swapchain using vkb
    vkb::SwapchainBuilder vkbSwapchainBuilder{mPhysicalDevice, mDevice, mSurface};

    VkSurfaceFormatKHR surfaceFormat{};
    surfaceFormat.format = mSwapchainImageFormat;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::Swapchain vkbSwapchain = vkbSwapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR) // vsync present mode
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    // get swapchain and related images from vkbSwapchain
    mSwapchain = vkbSwapchain.swapchain;

    mSwapchainImages = vkbSwapchain.get_images().value();
    mSwapchainImageViews = vkbSwapchain.get_image_views().value();

    // set swapchain extent
    mSwapchainExtent.width = width;
    mSwapchainExtent.height = height;
}

void VulkanEngine::destroySwapchain() {
    // destroy swapchain resources
    for (auto& imageView : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, imageView, nullptr);
    }

    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);

    mSwapchainImages.clear();
    mSwapchainImageViews.clear();
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

    // create command pool for immediate commands
    VK_CHECK(vkCreateCommandPool(mDevice, &commandPoolInfo, nullptr, &mImmCommandPool));

    // allocate command buffer for immediate commands
    VkCommandBufferAllocateInfo allocInfo = vkinit::command_buffer_allocate_info(mImmCommandPool);
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &allocInfo, &mImmCommandBuffer));

    // add imm cmd resources to deletion queue
    mMainDeletionQueue.push([&]() {
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
    mMainDeletionQueue.push([&]() {
        vkDestroyFence(mDevice, mImmFence, nullptr);
    });
}

void VulkanEngine::initDescriptors() {
    // create descriptor pool
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    mDescriptorAllocator.initPool(mDevice, 10, sizes);

    // create descriptor layout
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        mDrawImageDescriptorLayout = builder.build(mDevice, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // allocate a descriptor set
    mDrawImageDescriptors = mDescriptorAllocator.allocate(mDevice, mDrawImageDescriptorLayout);

    DescriptorWriter writer;
    writer.writeImage(0, mDrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
          .updateSet(mDevice, mDrawImageDescriptors);


    // add descriptor allocator and layout to deletion queue
    mMainDeletionQueue.push([&]() {
        mDescriptorAllocator.destroyPool(mDevice);

        vkDestroyDescriptorSetLayout(mDevice, mDrawImageDescriptorLayout, nullptr);
    });

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // create a descriptor pool for each frame
        std::vector<DynamicDescriptorAllocator::PoolSizeRatio> poolRatios = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }
        };

        mFrames[i].descriptorAllocator = DynamicDescriptorAllocator();
        mFrames[i].descriptorAllocator.init(mDevice, 1000, poolRatios);

        mMainDeletionQueue.push([&, i]() {
            mFrames[i].descriptorAllocator.destroyPools(mDevice);
        });
    }

    {
        DescriptorLayoutBuilder builder;
        mGpuSceneDataDescriptorLayout = builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                                               .build(mDevice, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
}

void VulkanEngine::initPipelines() {
    initBackgroundPipelines();
    initMeshPipeline();
}

void VulkanEngine::initMeshPipeline() {
    VkShaderModule fragShader;
    if (!vkutil::loadShaderModule("shaders/colored_triangle.frag.spv", mDevice, &fragShader)) {
        fmt::println("error building colored triangle frag shader");
    }

    VkShaderModule vertShader;
    if (!vkutil::loadShaderModule("shaders/colored_triangle_mesh.vert.spv", mDevice, &vertShader)) {
        fmt::println("error building colored triangle vertex shader");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipeline_layout_create_info();
    pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutInfo, nullptr, &mMeshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    mMeshPipeline = pipelineBuilder.setLayout(mMeshPipelineLayout)
                                    .setShaders(vertShader, fragShader)
                                    .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                                    .setPolygonMode(VK_POLYGON_MODE_FILL)
                                    .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                                    .setMultisampling()
                                    .enableBlendingAdditive()
                                    .enableDepthTesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                                    .setColorAttachmentFormat(mDrawImage.imageFormat)
                                    .setDepthFormat(mDepthImage.imageFormat)
                                    .buildPipeline(mDevice);

    // cleanup
    vkDestroyShaderModule(mDevice, vertShader, nullptr);
    vkDestroyShaderModule(mDevice, fragShader, nullptr);

    mMainDeletionQueue.push([&]() {
        vkDestroyPipelineLayout(mDevice, mMeshPipelineLayout, nullptr);
        vkDestroyPipeline(mDevice, mMeshPipeline, nullptr);
    });
}

void VulkanEngine::initDefaultData() {
    // load test meshes
    mTestMeshes = loadGltfMeshes(this, "assets/basicmesh.glb").value();
}

void VulkanEngine::initBackgroundPipelines() {
    // create pipeline layout
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &mDrawImageDescriptorLayout;
    computeLayout.setLayoutCount = 1;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(mDevice, &computeLayout, nullptr, &mGradientPipelineLayout));
    
    // create pipeline
    VkShaderModule gradientShader;

    if (!vkutil::loadShaderModule("shaders/gradient_color.comp.spv", mDevice, &gradientShader)) {
        fmt::println("Error when building gradient shader\n");
    }

    VkShaderModule skyShader;

    if (!vkutil::loadShaderModule("shaders/sky.comp.spv", mDevice, &skyShader)) {
        fmt::println("Error when building sky shader\n");
    }


    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.pNext = nullptr;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = gradientShader;
	stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.layout = mGradientPipelineLayout;
	computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect gradientEffect{};
    gradientEffect.pipelineLayout = mGradientPipelineLayout;
    gradientEffect.name = "gradient";

    // default colors
    gradientEffect.pushConstants.data1 = glm::vec4(1, 0, 0, 1);
    gradientEffect.pushConstants.data2 = glm::vec4(0, 0, 1, 1);

    VK_CHECK(vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientEffect.pipeline));

    computePipelineCreateInfo.stage.module = skyShader;

    ComputeEffect skyEffect{};
    skyEffect.pipelineLayout = mGradientPipelineLayout;
    skyEffect.name = "sky";
    skyEffect.pushConstants.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(mDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &skyEffect.pipeline));

    backgroundEffects.push_back(gradientEffect);
    backgroundEffects.push_back(skyEffect);

    // destroy shader module
    vkDestroyShaderModule(mDevice, skyShader, nullptr);
    vkDestroyShaderModule(mDevice, gradientShader, nullptr);

    mMainDeletionQueue.push([=, this]() {
        vkDestroyPipelineLayout(mDevice, mGradientPipelineLayout, nullptr);
        vkDestroyPipeline(mDevice, skyEffect.pipeline, nullptr);
        vkDestroyPipeline(mDevice, gradientEffect.pipeline, nullptr);
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
    ImGui_ImplSDL2_InitForVulkan(mWindow);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = mInstance;
    initInfo.PhysicalDevice = mPhysicalDevice;
    initInfo.Device = mDevice;
    initInfo.Queue = mGraphicsQueue;
    initInfo.DescriptorPool = imguiDescPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    // dynamic rendering parameters
    initInfo.PipelineRenderingCreateInfo = {};
    initInfo.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &mSwapchainImageFormat;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    ImGui_ImplVulkan_CreateFontsTexture();

    // add imgui resources to deletion queue
    mMainDeletionQueue.push([=, this]() {
        ImGui_ImplVulkan_Shutdown();
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

    // free meshes
    for (auto& mesh : mTestMeshes) {
        destroyBuffer(mesh->meshBuffers.indexBuffer);
        destroyBuffer(mesh->meshBuffers.vertexBuffer);
    }

    mMainDeletionQueue.flush();

    destroySwapchain();

    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    vkDestroyDevice(mDevice, nullptr);

    vkb::destroy_debug_utils_messenger(mInstance, mDebugMessenger);
    vkDestroyInstance(mInstance, nullptr);

    SDL_DestroyWindow(mWindow);

    // clear engine pointer
    gLoadedEngine = nullptr;
}

void VulkanEngine::resizeSwapchain() {
    vkDeviceWaitIdle(mDevice);

    destroySwapchain();

    int w, h;
    SDL_GetWindowSize(mWindow, &w, &h);

    // get surface capabilities to correct window size (SDL bug)
    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCapabilities));

    mWindowExtent.width = SDL_clamp(w, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
    mWindowExtent.height = SDL_clamp(h, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

    createSwapchain(mWindowExtent.width, mWindowExtent.height);
    fmt::println("resized swapchain");

    mResizeRequested = false;
}

void VulkanEngine::draw() {
    // wait until gpu finishes rendering last frame
    VK_CHECK(vkWaitForFences(mDevice, 1, &getCurrentFrame().renderFence, true, 1e9));

    // flush deletion queue of current frame
    getCurrentFrame().deletionQueue.flush();

    // clear descriptor pools for current frame
    getCurrentFrame().descriptorAllocator.clearPools(mDevice);

    // request image from swapchain
    uint32_t swapchainImageIndex;
    VkResult e = vkAcquireNextImageKHR(
        mDevice,
        mSwapchain,
        1e9,
        getCurrentFrame().swapchainSemaphore,
        nullptr,
        &swapchainImageIndex
        );

    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        mResizeRequested = true;

        vkDestroySemaphore(mDevice, getCurrentFrame().swapchainSemaphore, nullptr);
        VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
        VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &getCurrentFrame().swapchainSemaphore));
    
        return;
    }

    // set draw extent
    mDrawExtent.width = std::min(mSwapchainExtent.width, mDrawImage.imageExtent.width) * mRenderScale;
    mDrawExtent.height = std::min(mSwapchainExtent.height, mDrawImage.imageExtent.height) * mRenderScale;
    
    VK_CHECK(vkResetFences(mDevice, 1, &getCurrentFrame().renderFence));

    // get command buffer from current frame
    VkCommandBuffer commandBuffer = getCurrentFrame().mainCommandBuffer;

    // reset command buffer
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0));

    // begin recording command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

    // transition draw image to a writeable layout
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(commandBuffer);

    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(commandBuffer, mDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(commandBuffer);

    // transition draw and swapchain image into transfer layouts
    vkutil::transitionImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(commandBuffer, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy draw image to swapchain image
    vkutil::copyImageToImage(commandBuffer, mDrawImage.image, mSwapchainImages[swapchainImageIndex], mDrawExtent, mSwapchainExtent);

    // transition swapchain image to Attachment Optimal so we can draw directly to it
    vkutil::transitionImage(commandBuffer, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    // draw ImGui into the swapchain image
    drawImGui(commandBuffer, mSwapchainImageViews[swapchainImageIndex]);

    // transition swapchain image into a presentable layout
    vkutil::transitionImage(commandBuffer, mSwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    // finalize command buffer
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // prepare to submit command buffer to queue
    VkCommandBufferSubmitInfo commandBufferSubmitInfo = vkinit::command_buffer_submit_info(commandBuffer);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

    VkSubmitInfo2 submitInfo = vkinit::submit_info(&commandBufferSubmitInfo, &signalInfo, &waitInfo);

    // submit command buffer to queue
    VK_CHECK(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

    // present image
    VkPresentInfoKHR presentInfo = vkinit::present_info(&mSwapchain, &getCurrentFrame().renderSemaphore, &swapchainImageIndex);

    e = vkQueuePresentKHR(mGraphicsQueue, &presentInfo);

    if (e == VK_ERROR_OUT_OF_DATE_KHR || e == VK_SUBOPTIMAL_KHR) {
        mResizeRequested = true;

        // vkDestroySemaphore(mDevice, getCurrentFrame().swapchainSemaphore, nullptr);
        // VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
        // VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &getCurrentFrame().swapchainSemaphore));
        // return;
    }

    // increment frame number
    mFrameNumber++;
}

void VulkanEngine::drawBackground(VkCommandBuffer commandBuffer) {
    // // set clear color
    // VkClearColorValue clearValue;
    // float flash = std::abs(std::sin(mFrameNumber / 120.f));
    // clearValue = { {flash, 0.0f, flash, 1.0f} };

    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    // // clear image
    // vkCmdClearColorImage(commandBuffer, mDrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

    // bind gradient pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind descriptor set containing the draw image
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        mGradientPipelineLayout,
        0,
        1,
        &mDrawImageDescriptors,
        0,
        nullptr
        );

    vkCmdPushConstants(commandBuffer, mGradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.pushConstants);

    // execute compute shader
    vkCmdDispatch(commandBuffer, std::ceil(mDrawExtent.width / 16.0), std::ceil(mDrawExtent.height / 16.0), 1);
}

void VulkanEngine::drawImGui(VkCommandBuffer commandBuffer, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(mSwapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(commandBuffer, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    vkCmdEndRendering(commandBuffer);
}

void VulkanEngine::run() {
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            switch (e.type) {
            // close the window when user alt-f4s or clicks the X button
            case SDL_QUIT:
                bQuit = true;
                break;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    mStopRendering = true;
                }

                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    mStopRendering = false;
                }

                // if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                //     fmt::println("SDL got resize");
                //     mResizeRequested = true;
                // }

                break;

            default:
                break;
            }

            // send SDL event to imgui
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (mStopRendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (mResizeRequested) {
            resizeSwapchain();
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // demo window
        // ImGui::ShowDemoWindow();

        if (ImGui::Begin("background")) {
            ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];

            ImGui::SliderFloat("Render Scale", &mRenderScale, 0.3f, 1.f);

            ImGui::Text("Selected effect: ", effect.name.c_str());

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)& effect.pushConstants.data1);
            ImGui::InputFloat4("data2", (float*)& effect.pushConstants.data2);
            ImGui::InputFloat4("data3", (float*)& effect.pushConstants.data3);
            ImGui::InputFloat4("data4", (float*)& effect.pushConstants.data4);
        }

        ImGui::End();

        // render ImGui
        ImGui::Render();

        draw();
    }
}

void VulkanEngine::drawGeometry(VkCommandBuffer commandBuffer) {
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(mDrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(mDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(mDrawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(commandBuffer, &renderInfo);

    // bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mMeshPipeline);

    // set dynamic viewport and scissor
    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = mDrawExtent.width;
    viewport.height = mDrawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = viewport.width;
    scissor.extent.height = viewport.height;

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // allocate a new uniform buffer for the scene data
    AllocatedBuffer gpuSceneDataBuffer = createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    // add buffer to deletion queue
    getCurrentFrame().deletionQueue.push([=, this]() {
        destroyBuffer(gpuSceneDataBuffer);
    });

    // write buffer
    GPUSceneData* sceneData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    
    // TODO: add data
    // sceneData-> ....

    // create Descriptor set that binds the buffer
    VkDescriptorSet globalDescriptorSet = getCurrentFrame().descriptorAllocator.allocate(mDevice, mGpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
          .updateSet(mDevice, globalDescriptorSet);

    GPUDrawPushConstants pushConstants;
    pushConstants.worldMatrix = glm::mat4(1.f);
    pushConstants.vertexBuffer = mTestMeshes[2]->meshBuffers.vertexBufferAddress;

    glm::mat4 view = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -5.f));
    glm::mat4 projection = glm::perspective(glm::radians(70.f), mDrawExtent.width / (float)mDrawExtent.height, 10000.f, 0.1f);

    // invert Y direction
    projection[1][1] *= -1;

    pushConstants.worldMatrix = projection * view;

    vkCmdPushConstants(commandBuffer, mMeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);
    vkCmdBindIndexBuffer(commandBuffer, mTestMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, mTestMeshes[2]->surfaces[0].count, 1, mTestMeshes[2]->surfaces[0].startIndex, 0, 0);

    vkCmdEndRendering(commandBuffer);
}
