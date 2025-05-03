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
    uint32_t mipLevels;
};

struct ComputePushConstants {
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
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

struct alignas(16) Vertex {
    glm::vec3 position;
    float uvX;
    glm::vec3 normal;
    float uvY;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct PBRPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct SpritePushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct SkyboxPushConstants {
    glm::mat4 projViewMatrix;
    VkDeviceAddress vertexBuffer;
};

struct SkyboxLoadPushConstants {
    glm::mat4 projViewMatrix;
    VkDeviceAddress vertexBuffer;
    uint32_t mipLevel;
    uint32_t totalMips;
};

enum class MaterialPass : uint8_t {
    Opaque,
    Transparent,
    OpaqueDoubleSided,
    TransparentDoubleSided,
    Other
};

struct Pipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    Pipeline* pipeline;
    MaterialPass passType;

    VkDeviceSize descriptorOffset;
};

struct alignas(16) MaterialConstants {
    glm::vec4 colorFactors;
    glm::vec4 metalRoughFactors;
    glm::vec4 emissiveFactors;
    float emissiveStrength;
    float normalScale;
    float occlusionStrength;
};

struct MaterialResources {
    AllocatedImage colorImage;
    VkSampler colorSampler;

    AllocatedImage metalRoughImage;
    VkSampler metalRoughSampler;

    AllocatedImage normalImage;
    VkSampler normalSampler;

    AllocatedImage emissiveImage;
    VkSampler emissiveSampler;

    AllocatedImage occlusionImage;
    VkSampler occlusionSampler;

    VkBuffer dataBuffer;
    uint32_t dataBufferOffset;
    VkDeviceAddress dataBufferAddress;
};

struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct GLTFRenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;

    Bounds bounds;
};

struct SpriteRenderObject {
    AllocatedImage* image;
    glm::mat4 transform;
};

struct SkyboxAsset {
    std::string name;

    AllocatedImage hdrImage;
    AllocatedImage envMap;
    AllocatedImage irrMap;
    AllocatedImage prefilteredEnvMap;
    AllocatedImage brdfLut;

    static constexpr VkExtent2D ENV_MAP_SIZE = {1024, 1024};
    static constexpr VkExtent2D IRR_MAP_SIZE = {32, 32};
    static constexpr VkExtent2D PREFILTERED_ENV_MAP_SIZE = {512, 512};
    static constexpr VkExtent2D BRDF_LUT_SIZE = {512, 512};
};

struct SceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColor;
    glm::vec4 viewPosition;
    glm::vec4 data;
};

struct RenderContext {
    std::vector<GLTFRenderObject> opaqueObjects;
    std::vector<GLTFRenderObject> transparentObjects;
    std::vector<SpriteRenderObject> sprites;

    SceneData sceneData;
    SkyboxAsset *skybox = nullptr;
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
