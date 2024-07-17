// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>

#include <vulkan/vulkan.h>
#include "vk_descriptors.h"
#include "vk_enum_string_helper.h"
#include <vk_mem_alloc.h>
#include "deletion_queue.h"

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan_core.h>

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	VkSemaphore swapchainSemaphore;
	VkSemaphore renderSemaphore;
	VkFence renderFence;

	DeletionQueue deletionQueue;
    DynamicDescriptorAllocator descriptorAllocator;
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect {
    std::string name;

    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    ComputePushConstants pushConstants;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocInfo;
};

struct Vertex {
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
    glm::vec4 color;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)