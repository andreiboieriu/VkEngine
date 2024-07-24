// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include "volk.h"

#include <memory>
#include <string>
#include <vector>

#include <vk_mem_alloc.h>
#include "deletion_queue.h"
#include "vk_enum_string_helper.h"

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

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
    VkDeviceAddress deviceAddress;
};


struct DescriptorData {
    VkDescriptorSetLayout layout;
    AllocatedBuffer buffer;
    VkDeviceSize size;
    VkDeviceSize offset;
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

enum class MaterialPass : uint8_t {
    Opaque,
    Transparent,
    Other
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    MaterialPass passType;

    VkDeviceSize descriptorOffset;
};

struct MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metalRoughFactors;

    // padding
    glm::vec4 extra[14];
};

struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;
    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;
    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
    VkDeviceAddress dataBufferAddress;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;

    Bounds bounds;
};

struct RenderContext {
    std::vector<RenderObject> opaqueObjects;
    std::vector<RenderObject> transparentObjects;
};

struct GLTFMaterial {
    MaterialInstance materialInstance;
};

struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    std::shared_ptr<GLTFMaterial> material;

    Bounds bounds;
};

struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

inline VkDeviceSize alignedSize(VkDeviceSize value, VkDeviceSize alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)