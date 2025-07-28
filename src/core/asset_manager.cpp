#include "asset_manager.h"
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "vk_engine.h"
#include "vk_loader.h"
#include "vk_images.h"
#include "vk_initializers.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include <glm/gtc/packing.hpp>

#include <imgui_impl_vulkan.h>
#include <ktx.h>
#include <ktxvulkan.h>

AssetManager::AssetManager(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    loadDefaultSamplers();
    loadDefaultMeshes();
    loadDefaultImages();
    loadImguiTextures();

    std::filesystem::path path = mProjectPath + "/assets/compute_effects/textures";

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadImage(entry.path().filename(), VK_FORMAT_R8G8B8A8_UNORM, ImageType::COMPUTE);
    }
}

AssetManager::~AssetManager() {
    freeResources();
}

void AssetManager::loadAllAssets() {
    std::filesystem::path path = mProjectPath + "/assets/sprites";

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadImage(entry.path().filename(), VK_FORMAT_R8G8B8A8_SRGB, ImageType::SPRITE);
    }

    path = mProjectPath + "/assets/compute_effects/textures";

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadImage(entry.path().filename(), VK_FORMAT_R8G8B8A8_UNORM, ImageType::COMPUTE);
    }

    path = mProjectPath + "/assets/gltfs";

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadGltf(entry.path().filename());
    }

    loadBrdfLut();

    path = mProjectPath + "/assets/skyboxes";

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (std::filesystem::is_directory(entry.path()) || !entry.path().has_extension() || entry.path().extension() != ".hdr") {
            continue;
        }

        loadSkybox(entry.path().filename());
    }
}

void AssetManager::loadImage(const std::filesystem::path& filename, VkFormat format, ImageType type) {
    std::filesystem::path filePath = mProjectPath + SPRITES_PATH;
    auto* target = &mImages;

    if (type == ImageType::COMPUTE) {
        target = &mComputeImages;
        filePath = mProjectPath + COMPUTE_IMAGES_PATH;
    } else if (type == ImageType::IMGUI) {
        filePath = IMGUI_PATH;
    }

    if (target->contains(filename)) {
        return;
    }

    filePath += filename.string();
    fmt::println("Loading image: {}", filename.c_str());

    // check if file exists
    if (!std::filesystem::exists(filePath)) {
        fmt::println("Failed to find texture file: {}", filePath.string());
        return;
    }

    // load file using stb
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height , &channels, 4);

    if (!data) {
        fmt::println("Failed to load image using stb: {}", filePath.string());
        return;
    }

    // create image resource
    auto newImage = mVkEngine.createImage(
        data,
        VkExtent3D{(uint32_t)width, (uint32_t)height, 1},
        format,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false
    );

    stbi_image_free(data);

    if (type == ImageType::COMPUTE) {
        mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
            vkutil::transitionImage(commandBuffer, newImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        }, true);
    }

    if (type == ImageType::IMGUI) {
        ImTextureID id = ImGui_ImplVulkan_AddTexture(
            mDefaultSamplers["linear"],
            newImage.imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        mImguiImages[filename] = {newImage, id};
    } else {
        (*target)[filename] = newImage;
    }
}

void AssetManager::loadComputeEffectTextures() {
    static const std::filesystem::path path(mProjectPath + "/assets/compute_effects/textures");

    if (!std::filesystem::exists(path)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadImage(entry.path().filename(), VK_FORMAT_R8G8B8A8_UNORM, ImageType::COMPUTE);

        mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
            vkutil::transitionImage(commandBuffer, mComputeImages[entry.path().stem()].image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        });
    }
}

void AssetManager::loadImguiTextures() {
    static const std::filesystem::path path("assets/imgui");

    if (!std::filesystem::exists(path)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        loadImage(entry.path().filename(), VK_FORMAT_R8G8B8A8_SRGB, ImageType::IMGUI);
    }
}

void AssetManager::loadGltf(const std::string& name) {
    if (mLoadedGltfs.contains(name)) {
        return;
    }

    std::filesystem::path filePath = mProjectPath + GLTFS_PATH + name;

    // check if file exists
    if (!std::filesystem::exists(filePath)) {
        fmt::println("Failed to find gltf file: {}", filePath.string());
        return;
    }

    fmt::println("Loading GLTF: {}", name);

    mLoadedGltfs[name] = std::make_unique<LoadedGLTF>(filePath, mVkEngine);
}

void AssetManager::renderToCubemap(
    AllocatedImage& src,
    AllocatedImage& dest,
    Pipeline *pipeline,
    VkExtent2D destSize,
    uint32_t mipLevel,
    bool flipViewport
) {
    // setup projection and view matrices
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    if (!flipViewport) {
        glm::mat4 temp = captureViews[2];
        captureViews[2] = captureViews[3];
        captureViews[3] = temp;
    }

    // modify size according to mip level
    destSize.width >>= mipLevel;
    destSize.height >>= mipLevel;

    auto cubeMesh = mMeshes["cube"];

    // create push constants
    SkyboxLoadPushConstants captureConstants{};
    captureConstants.vertexBuffer = cubeMesh.meshBuffers.vertexBufferAddress;
    captureConstants.mipLevel = mipLevel;
    captureConstants.totalMips = dest.mipLevels;

    // transition cubemap to color attachment layout
    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, dest.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    });

    for (uint32_t i = 0; i < 6; i++) {
        captureConstants.projViewMatrix = captureProjection * glm::mat4(glm::mat3(captureViews[i]));

        // create cubemap face view
        VkImageView cubeFaceView;

        VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(VK_FORMAT_R16G16B16A16_SFLOAT, dest.image, VK_IMAGE_ASPECT_COLOR_BIT);
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.baseMipLevel = mipLevel;
        viewInfo.subresourceRange.levelCount = 1;

        VK_CHECK(vkCreateImageView(mVkEngine.getDevice(), &viewInfo, nullptr, &cubeFaceView));

        // render to cube face
        mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
            VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(cubeFaceView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

            VkRenderingInfo renderInfo = vkinit::rendering_info(destSize, &colorAttachment, nullptr);

            vkCmdBeginRendering(commandBuffer, &renderInfo);

            // set dynamic viewport and scissor
            VkViewport viewport{};
            viewport.x = 0.f;
            viewport.y = flipViewport ? destSize.height : 0.f;
            viewport.width = destSize.width;
            viewport.height = flipViewport ? -(float)(destSize.height) : destSize.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            scissor.extent.width = destSize.width;
            scissor.extent.height = destSize.height;

            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->pipeline);
            // bind index buffer
            vkCmdBindIndexBuffer(commandBuffer, cubeMesh.meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            // push descriptor set
            DescriptorWriter writer;
            std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, src.imageView, mDefaultSamplers["linear"], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                                        .getWrites();

            vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

            // push constants
            vkCmdPushConstants(commandBuffer, pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SkyboxLoadPushConstants), &captureConstants);

            // draw
            vkCmdDrawIndexed(commandBuffer, cubeMesh.surfaces[0].count, 1, cubeMesh.surfaces[0].startIndex, 0, 0);

            vkCmdEndRendering(commandBuffer);
        }, true);

        // destroy cubemap face view
        vkDestroyImageView(mVkEngine.getDevice(), cubeFaceView, nullptr);
    }

    // transition cubemap to shader read layout
    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, dest.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });
}

void AssetManager::saveSkyboxKtx(const std::string& name) {
    if (!mSkyboxes.contains(name)) {
        fmt::println("Error: attempting to save non-loaded skybox {} to ktx file.", name);
        return;
    }

    auto& skybox = mSkyboxes[name];

    if (!std::filesystem::exists(mProjectPath + "/assets/skyboxes/brdf_lut.ktx2")) {
        auto& image = skybox.brdfLut;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        createInfo.baseWidth = image.imageExtent.width;
        createInfo.baseHeight = image.imageExtent.height;
        createInfo.baseDepth = 1;
        createInfo.numLevels = image.mipLevels;
        createInfo.numLayers = 1;
        createInfo.numFaces = 1;
        createInfo.isArray = false;
        createInfo.numDimensions = 2;
        ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);

        uint32_t size = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent.width = image.imageExtent.width;
            copyRegion.imageExtent.height = image.imageExtent.height;
            copyRegion.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);

            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);

        void* data = mVkEngine.getDataFromBuffer(staging);
        memcpy(texture->pData, data, size);

        mVkEngine.destroyBuffer(staging);

        std::string path = mProjectPath + "/assets/skyboxes/brdf_lut.ktx2";
        ktxTexture2_WriteToNamedFile(texture, path.c_str());
        ktxTexture2_Destroy(texture);
    }

    std::string stem = std::filesystem::path(name).stem();

    if (!std::filesystem::exists(mProjectPath + "/assets/skyboxes/" + stem)) {
        std::filesystem::create_directory(mProjectPath + "/assets/skyboxes/" + stem);
    }

    if (!std::filesystem::exists(mProjectPath + "/assets/skyboxes/" + stem + "/irradiance.ktx2")) {
        auto& image = skybox.irrMap;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        createInfo.baseWidth = image.imageExtent.width;
        createInfo.baseHeight = image.imageExtent.height;
        createInfo.baseDepth = 1;
        createInfo.numLevels = image.mipLevels;
        createInfo.numLayers = 6;
        createInfo.numFaces = 1;
        createInfo.isArray = true;
        createInfo.numDimensions = 2;

        if (ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) != KTX_SUCCESS) {
            fmt::println("Error: Failed to create ktx irr texture {}", image.mipLevels);
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
        uint32_t size = faceSize * 6;
        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 6;
            copyRegion.imageExtent.width = image.imageExtent.width;
            copyRegion.imageExtent.height = image.imageExtent.height;
            copyRegion.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);


        for (int face = 0; face < 6; face++) {
            ktx_size_t offset;
            ktxTexture2_GetImageOffset(texture, 0, face, 0, &offset);
            void* data = static_cast<uint8_t*>(texture->pData) + offset;
            void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + faceSize * face;

            memcpy(data, stagingData, faceSize);
        }

        mVkEngine.destroyBuffer(staging);

        std::string path = mProjectPath + "/assets/skyboxes/" + stem + "/irradiance.ktx2";
        ktxTexture2_WriteToNamedFile(texture, path.c_str());
        ktxTexture_Destroy(ktxTexture(texture));
    }

    if (!std::filesystem::exists(mProjectPath + "/assets/skyboxes/" + stem + "/environment.ktx2")) {
        auto& image = skybox.envMap;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        createInfo.baseWidth = image.imageExtent.width;
        createInfo.baseHeight = image.imageExtent.height;
        createInfo.baseDepth = 1;
        createInfo.numLevels = image.mipLevels;
        createInfo.numLayers = 6;
        createInfo.numFaces = 1;
        createInfo.isArray = true;
        createInfo.numDimensions = 2;

        if (ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) != KTX_SUCCESS) {
            fmt::println("Error: Failed to create ktx irr texture {}", image.mipLevels);
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
        uint32_t size = faceSize * 6;
        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        for (int mip = 0; mip < image.mipLevels; mip++) {
            mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
                if (mip == 0)
                    vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = mip;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 6;
                copyRegion.imageExtent.width = image.imageExtent.width >> mip;
                copyRegion.imageExtent.height = image.imageExtent.height >> mip;
                copyRegion.imageExtent.depth = 1;
                vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);

                if (mip == image.mipLevels - 1)
                    vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }, true);

            for (int face = 0; face < 6; face++) {
                size_t adjustedFaceSize = faceSize >> (2 * mip);

                ktx_size_t offset;
                ktxTexture2_GetImageOffset(texture, mip, face, 0, &offset);
                void* data = static_cast<uint8_t*>(texture->pData) + offset;
                void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + adjustedFaceSize * face;

                memcpy(data, stagingData, adjustedFaceSize);
            }
        }

        mVkEngine.destroyBuffer(staging);

        std::string path = mProjectPath + "/assets/skyboxes/" + stem + "/environment.ktx2";
        ktxTexture2_WriteToNamedFile(texture, path.c_str());
        ktxTexture_Destroy(ktxTexture(texture));
    }

    if (!std::filesystem::exists(mProjectPath + "/assets/skyboxes/" + stem + "/prefiltered.ktx2")) {
        auto& image = skybox.prefilteredEnvMap;

        ktxTexture2* texture;
        ktxTextureCreateInfo createInfo = {};
        createInfo.vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        createInfo.baseWidth = image.imageExtent.width;
        createInfo.baseHeight = image.imageExtent.height;
        createInfo.baseDepth = 1;
        createInfo.numLevels = 5;
        createInfo.numLayers = 6;
        createInfo.numFaces = 1;
        createInfo.isArray = true;
        createInfo.numDimensions = 2;

        if (ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture) != KTX_SUCCESS) {
            fmt::println("Error: Failed to create ktx irr texture {}", image.mipLevels);
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
        uint32_t size = faceSize * 6;
        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        for (int mip = 0; mip < 5; mip++) {
            mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
                if (mip == 0)
                    vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = mip;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 6;
                copyRegion.imageExtent.width = image.imageExtent.width >> mip;
                copyRegion.imageExtent.height = image.imageExtent.height >> mip;
                copyRegion.imageExtent.depth = 1;
                vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);

                if (mip == 4)
                    vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }, true);

            for (int face = 0; face < 6; face++) {
                size_t adjustedFaceSize = faceSize >> (2 * mip);

                ktx_size_t offset;
                ktxTexture2_GetImageOffset(texture, mip, face, 0, &offset);
                void* data = static_cast<uint8_t*>(texture->pData) + offset;
                void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + adjustedFaceSize * face;

                memcpy(data, stagingData, adjustedFaceSize);
            }
        }

        mVkEngine.destroyBuffer(staging);

        std::string path = mProjectPath + "/assets/skyboxes/" + stem + "/prefiltered.ktx2";
        ktxTexture2_WriteToNamedFile(texture, path.c_str());
        ktxTexture_Destroy(ktxTexture(texture));
    }
}

void AssetManager::saveSkyboxToFile(const SkyboxAsset& skybox) {
    {
        auto& image = skybox.brdfLut;

        uint32_t size = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }, true);

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent.width = image.imageExtent.width;
            copyRegion.imageExtent.height = image.imageExtent.height;
            copyRegion.imageExtent.depth = 1;
            vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);
        }, true);

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);

        void* data = mVkEngine.getDataFromBuffer(staging);

        uint32_t byteCount = image.imageExtent.width * image.imageExtent.height * 4;
        std::vector<uint8_t> dataVec(byteCount, 0);

        for (uint32_t i = 0; i < byteCount; i ++) {
            dataVec[i] = glm::clamp(glm::unpackHalf1x16(((uint16_t*)data)[i]), 0.f, 1.f) * 255;
        }

        stbi_write_png(
            "test_files/brdflut.png",
            image.imageExtent.width,
            image.imageExtent.height,
            4,
            dataVec.data(),
            image.imageExtent.width * 4
        );

        mVkEngine.destroyBuffer(staging);
    }

    {
        auto& image = skybox.irrMap;

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }, true);

        for (int i = 0; i < 6; i++) {
            uint32_t size = vkutil::getPixelSize(image.imageFormat) * image.imageExtent.width * image.imageExtent.height;
            AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

            mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = i;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageExtent.width = image.imageExtent.width;
                copyRegion.imageExtent.height = image.imageExtent.height;
                copyRegion.imageExtent.depth = 1;
                vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);
            }, true);

            void* data = mVkEngine.getDataFromBuffer(staging);

            uint32_t byteCount = image.imageExtent.width * image.imageExtent.height * 4;
            std::vector<uint8_t> dataVec(byteCount, 0);

            for (uint32_t i = 0; i < byteCount; i ++) {
                dataVec[i] = glm::clamp(glm::unpackHalf1x16(((uint16_t*)data)[i]), 0.f, 1.f) * 255;
            }

            stbi_write_png(
                ("test_files/irr_" + std::to_string(i) + ".png").c_str(),
                image.imageExtent.width,
                image.imageExtent.height,
                4,
                dataVec.data(),
                image.imageExtent.width * 4
            );

            mVkEngine.destroyBuffer(staging);
        }

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);
    }

    {
        auto& image = skybox.prefilteredEnvMap;

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }, true);

        for (uint32_t mip = 0; mip < 5; mip++) {
            for (int i = 0; i < 6; i++) {
                uint32_t width = image.imageExtent.width >> mip;
                uint32_t height = image.imageExtent.height >> mip;

                uint32_t size = vkutil::getPixelSize(image.imageFormat) * width * height;
                AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

                mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
                    VkBufferImageCopy copyRegion = {};
                    copyRegion.bufferOffset = 0;
                    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    copyRegion.imageSubresource.mipLevel = mip;
                    copyRegion.imageSubresource.baseArrayLayer = i;
                    copyRegion.imageSubresource.layerCount = 1;
                    copyRegion.imageExtent.width = width;
                    copyRegion.imageExtent.height = height;
                    copyRegion.imageExtent.depth = 1;
                    vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);
                }, true);

                void* data = mVkEngine.getDataFromBuffer(staging);

                uint32_t byteCount = width * height * 4;
                std::vector<uint8_t> dataVec(byteCount, 0);

                for (uint32_t i = 0; i < byteCount; i ++) {
                    dataVec[i] = glm::clamp(glm::unpackHalf1x16(((uint16_t*)data)[i]), 0.f, 1.f) * 255;
                }

                stbi_write_png(
                    ("test_files/prefiltered_" + std::to_string(i) + "mip_" + std::to_string(mip) + ".png").c_str(),
                    width,
                    height,
                    4,
                    dataVec.data(),
                    width * 4
                );

                mVkEngine.destroyBuffer(staging);
            }
        }

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);
    }

    {
        auto& image = skybox.envMap;

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        }, true);

        for (uint32_t mip = 0; mip < skybox.envMap.mipLevels; mip++) {
            for (int i = 0; i < 6; i++) {
                uint32_t width = image.imageExtent.width >> mip;
                uint32_t height = image.imageExtent.height >> mip;

                uint32_t size = vkutil::getPixelSize(image.imageFormat) * width * height;
                AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

                mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
                    VkBufferImageCopy copyRegion = {};
                    copyRegion.bufferOffset = 0;
                    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    copyRegion.imageSubresource.mipLevel = mip;
                    copyRegion.imageSubresource.baseArrayLayer = i;
                    copyRegion.imageSubresource.layerCount = 1;
                    copyRegion.imageExtent.width = width;
                    copyRegion.imageExtent.height = height;
                    copyRegion.imageExtent.depth = 1;
                    vkCmdCopyImageToBuffer(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);
                }, true);

                void* data = mVkEngine.getDataFromBuffer(staging);

                uint32_t byteCount = width * height * 4;
                std::vector<uint8_t> dataVec(byteCount, 0);

                for (uint32_t i = 0; i < byteCount; i ++) {
                    dataVec[i] = glm::clamp(glm::unpackHalf1x16(((uint16_t*)data)[i]), 0.f, 1.f) * 255;
                }

                stbi_write_png(
                    ("test_files/environment_" + std::to_string(i) + "mip_" + std::to_string(mip) + ".png").c_str(),
                    width,
                    height,
                    4,
                    dataVec.data(),
                    width * 4
                );

                mVkEngine.destroyBuffer(staging);
            }
        }

        mVkEngine.immediateSubmit([&](VkCommandBuffer cmd) {
            vkutil::transitionImage(cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);
    }
}

void AssetManager::loadSkybox(const std::string& name) {
    if (mSkyboxes.contains(name)) {
        return;
    }

    if (!mDefaultImages.contains("brdflut")) {
        loadBrdfLut();
    }

    fmt::println("Loading skybox: {}", name);
    std::filesystem::path filePath = mProjectPath + SKYBOXES_PATH + name;

    std::filesystem::path precomputedPath = mProjectPath + SKYBOXES_PATH + filePath.stem().string();
    bool precomputed = std::filesystem::exists(precomputedPath) && std::filesystem::is_directory(precomputedPath);

    SkyboxAsset skybox{};
    skybox.name = name;

    // create hdr image
    if (!precomputed) {
        if (!std::filesystem::exists(filePath)) {
            fmt::println("failed to find skybox file: {}", filePath.string());
            return;
        }

        if (!filePath.filename().string().ends_with(".hdr")) {
            fmt::println("unsupported skybox format: {}", filePath.string());
            return;
        }

        // load hdr image using stb
        int width, height, nrChannels;
        float* data = stbi_loadf(filePath.string().c_str(), &width, &height, &nrChannels, 4);

        if (!data) {
            fmt::println("failed to load skybox file using stb: {}", filePath.string());
            return;
        }

        skybox.hdrImage = mVkEngine.createImage(
            data,
            VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_SAMPLED_BIT,
            false
        );
    }

    // create environment map
    skybox.envMap = mVkEngine.createCubemap(
        SkyboxAsset::ENV_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        std::nullopt,
        true
    );

    if (precomputed) {
        std::string path = precomputedPath.string() + "/environment.ktx2";
        ktxTexture2* texture;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS) {
            fmt::println("Failed to load environment map from ktx2 file.");
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(skybox.envMap.imageFormat) * skybox.envMap.imageExtent.width * skybox.envMap.imageExtent.height;
        uint32_t size = faceSize * 6;

        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        for (int mip = 0; mip < skybox.envMap.mipLevels; mip++) {
            for (int face = 0; face < 6; face++) {
                size_t adjustedFaceSize = faceSize >> (2 * mip);

                ktx_size_t offset;
                ktxTexture2_GetImageOffset(texture, mip, face, 0, &offset);
                void* data = static_cast<uint8_t*>(texture->pData) + offset;

                void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + adjustedFaceSize * face;
                memcpy(stagingData, data, adjustedFaceSize);
            }

            mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
                if (mip == 0)
                    vkutil::transitionImage(commandBuffer, skybox.envMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkBufferImageCopy copyRegion{};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0; // Tightly packed
                copyRegion.bufferImageHeight = 0;

                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = mip;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 6;

                copyRegion.imageOffset = {0, 0, 0};
                copyRegion.imageExtent = {
                    static_cast<uint32_t>(texture->baseWidth >> mip),
                    static_cast<uint32_t>(texture->baseHeight >> mip),
                    1
                };

                vkCmdCopyBufferToImage(
                    commandBuffer,
                    staging.buffer,
                    skybox.envMap.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion
                );

                if (mip == skybox.envMap.mipLevels - 1)
                    vkutil::transitionImage(commandBuffer, skybox.envMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }, true);
        }

        mVkEngine.destroyBuffer(staging);
        ktxTexture2_Destroy(texture);
    } else {
        for (int i = 0; i < skybox.envMap.mipLevels; i++) {
            renderToCubemap(
                skybox.hdrImage,
                skybox.envMap,
                mVkEngine.getPipelineResourceManager().getPipeline(PipelineResourceManager::PipelineType::EQUI_TO_CUBE),
                SkyboxAsset::ENV_MAP_SIZE,
                i,
                true
            );
        }
    }

    // create irradiance cube map
    skybox.irrMap = mVkEngine.createCubemap(
        SkyboxAsset::IRR_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    );

    if (precomputed) {
        std::string path = precomputedPath.string() + "/irradiance.ktx2";
        ktxTexture2* texture;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS) {
            fmt::println("Failed to load irradiance map from ktx2 file.");
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(skybox.irrMap.imageFormat) * skybox.irrMap.imageExtent.width * skybox.irrMap.imageExtent.height;
        uint32_t size = faceSize * 6;

        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        for (int face = 0; face < 6; face++) {
            ktx_size_t offset;
            ktxTexture2_GetImageOffset(texture, 0, face, 0, &offset);
            void* data = static_cast<uint8_t*>(texture->pData) + offset;

            void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + faceSize * face;
            memcpy(stagingData, data, faceSize);
        }

        mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
            vkutil::transitionImage(commandBuffer, skybox.irrMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0; // Tightly packed
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 6;

            copyRegion.imageOffset = {0, 0, 0};
            copyRegion.imageExtent = {
                static_cast<uint32_t>(texture->baseWidth),
                static_cast<uint32_t>(texture->baseHeight),
                1
            };

            vkCmdCopyBufferToImage(
                commandBuffer,
                staging.buffer,
                skybox.irrMap.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion
            );

            vkutil::transitionImage(commandBuffer, skybox.irrMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }, true);

        mVkEngine.destroyBuffer(staging);
        ktxTexture2_Destroy(texture);
    } else {
        renderToCubemap(
            skybox.envMap,
            skybox.irrMap,
            mVkEngine.getPipelineResourceManager().getPipeline(PipelineResourceManager::PipelineType::ENV_TO_IRR),
            SkyboxAsset::IRR_MAP_SIZE
        );
    }

    // create prefiltered environment map
    skybox.prefilteredEnvMap = mVkEngine.createCubemap(
        SkyboxAsset::PREFILTERED_ENV_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        std::nullopt,
        true
    );

    if (precomputed) {
        std::string path = precomputedPath.string() + "/prefiltered.ktx2";
        ktxTexture2* texture;
        KTX_error_code result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS) {
            fmt::println("Failed to load prefiltered map from ktx2 file.");
            return;
        }

        uint32_t faceSize = vkutil::getPixelSize(skybox.prefilteredEnvMap.imageFormat) * skybox.prefilteredEnvMap.imageExtent.width * skybox.prefilteredEnvMap.imageExtent.height;
        uint32_t size = faceSize * 6;

        AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        for (int mip = 0; mip < 5; mip++) {
            for (int face = 0; face < 6; face++) {
                size_t adjustedFaceSize = faceSize >> (2 * mip);

                ktx_size_t offset;
                ktxTexture2_GetImageOffset(texture, mip, face, 0, &offset);
                void* data = static_cast<uint8_t*>(texture->pData) + offset;

                void* stagingData = static_cast<uint8_t*>(mVkEngine.getDataFromBuffer(staging)) + adjustedFaceSize * face;
                memcpy(stagingData, data, adjustedFaceSize);
            }

            mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
                if (mip == 0)
                    vkutil::transitionImage(commandBuffer, skybox.prefilteredEnvMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkBufferImageCopy copyRegion{};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0; // Tightly packed
                copyRegion.bufferImageHeight = 0;

                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = mip;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 6;

                copyRegion.imageOffset = {0, 0, 0};
                copyRegion.imageExtent = {
                    static_cast<uint32_t>(texture->baseWidth >> mip),
                    static_cast<uint32_t>(texture->baseHeight >> mip),
                    1
                };

                vkCmdCopyBufferToImage(
                    commandBuffer,
                    staging.buffer,
                    skybox.prefilteredEnvMap.image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion
                );

                if (mip == 4)
                    vkutil::transitionImage(commandBuffer, skybox.prefilteredEnvMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }, true);
        }

        mVkEngine.destroyBuffer(staging);
        ktxTexture2_Destroy(texture);
    } else {
        for (uint32_t mip = 0; mip < 5; mip++) {
            renderToCubemap(
                skybox.envMap,
                skybox.prefilteredEnvMap,
                mVkEngine.getPipelineResourceManager().getPipeline(PipelineResourceManager::PipelineType::PREFILTER_ENV),
                SkyboxAsset::PREFILTERED_ENV_MAP_SIZE,
                mip
            );
        }
    }

    // set brdf lut image
    skybox.brdfLut = mDefaultImages["brdflut"];

    if (!std::filesystem::exists(precomputedPath.string() + "/metadata.json")) {
        computeDefaultDirectionalLight(skybox);

        std::ofstream file(precomputedPath.string() + "/metadata.json");

        nlohmann::json j{};
        j["lightDir"] = std::vector<float> {
            skybox.defaultDirectionalLight.x,
            skybox.defaultDirectionalLight.y,
            skybox.defaultDirectionalLight.z
        };

        file << j.dump(2);
        file.close();
    } else {
        std::ifstream file(precomputedPath.string() + "/metadata.json");
        nlohmann::json j{};
        file >> j;

        skybox.defaultDirectionalLight.x = j["lightDir"][0];
        skybox.defaultDirectionalLight.y = j["lightDir"][1];
        skybox.defaultDirectionalLight.z = j["lightDir"][2];

        file.close();
    }

    mSkyboxes[name] = skybox;

    // saveSkyboxToFile(skybox);
    saveSkyboxKtx(name);
}

void AssetManager::computeDefaultDirectionalLight(SkyboxAsset& skybox) {
    uint32_t mipLevel = 2;
    uint32_t faceWidth = skybox.envMap.imageExtent.width >> mipLevel;
    uint32_t faceHeight = skybox.envMap.imageExtent.height >> mipLevel;

    uint32_t faceSize = vkutil::getPixelSize(skybox.envMap.imageFormat) * faceWidth * faceHeight;
    uint32_t size = faceSize * 6;

    AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, skybox.envMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 2;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 6;
        copyRegion.imageExtent.width = faceWidth;
        copyRegion.imageExtent.height = faceHeight;
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyImageToBuffer(commandBuffer, skybox.envMap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copyRegion);

        vkutil::transitionImage(commandBuffer, skybox.envMap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }, true);

    void* data = mVkEngine.getDataFromBuffer(staging);
    float maxLuminance = 0.f;

    for (int face = 0; face < 6; face++) {
        for (int i = 0; i < faceWidth; i++) {
            for (int j = 0; j < faceHeight; j++) {
                uint32_t curr = (i * faceHeight + j) * 4 + 4 * (face * faceHeight * faceWidth);
                float x = glm::unpackHalf1x16(((uint16_t*)data)[curr]);
                float y = glm::unpackHalf1x16(((uint16_t*)data)[curr + 1]);
                float z = glm::unpackHalf1x16(((uint16_t*)data)[curr + 2]);

                float luminance = glm::sqrt(0.2126f * x + 0.7152f * y + 0.0722f * z);

                if (luminance > maxLuminance) {
                    float s = (i + 0.5f) / faceWidth * 2.0f - 1.0f;
                    float t = (j + 0.5f) / faceHeight * 2.0f - 1.0f;
                    // t = -t;

                    glm::vec3 lightDir;

                    switch (face) {
                        case 0: lightDir = glm::vec3( 1, -t, -s); break; // +X
                        case 1: lightDir = glm::vec3(-1, -t,  s); break; // -X
                        case 2: lightDir = glm::vec3( s,  1,  t); break; // +Y
                        case 3: lightDir = glm::vec3( s, -1, -t); break; // -Y
                        case 4: lightDir = glm::vec3( s, -t,  1); break; // +Z
                        case 5: lightDir = glm::vec3(-s, -t, -1); break; // -Z
                    }

                    lightDir = glm::normalize(lightDir);
                    skybox.defaultDirectionalLight = lightDir;

                    maxLuminance = luminance;
                }
            }
        }
    }

    fmt::println("Default light dir for skybox {}: {} {} {}", skybox.name, skybox.defaultDirectionalLight.x, skybox.defaultDirectionalLight.y, skybox.defaultDirectionalLight.z);

    mVkEngine.destroyBuffer(staging);
}

MeshAsset createQuadMesh(VulkanEngine& engine) {
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

    MeshAsset newMesh{};

    newMesh.meshBuffers = engine.uploadMesh(indices, vertices);
    newMesh.surfaces.push_back(GeoSurface{.startIndex = 0, .count = static_cast<uint32_t>(indices.size())});

    return newMesh;
}

MeshAsset createCubeMesh(VulkanEngine& engine){
    std::vector<Vertex> vertices = {
        Vertex{.position = glm::vec3(-1.0f, -1.0f, -1.0f)},
        Vertex{.position = glm::vec3(1.0f, -1.0f, -1.0f)},
        Vertex{.position = glm::vec3(1.0f, 1.0f, -1.0f)},
        Vertex{.position = glm::vec3(-1.0f, 1.0f, -1.0f)},
        Vertex{.position = glm::vec3(-1.0f, -1.0f, 1.0f)},
        Vertex{.position = glm::vec3(1.0f, -1.0f, 1.0f)},
        Vertex{.position = glm::vec3(1.0f, 1.0f, 1.0f)},
        Vertex{.position = glm::vec3(-1.0f, 1.0f, 1.0f)}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0, // Front face
        4, 5, 6, 6, 7, 4, // Back face
        0, 1, 5, 5, 4, 0, // Bottom face
        2, 3, 7, 7, 6, 2, // Top face
        0, 3, 7, 7, 4, 0, // Left face
        1, 2, 6, 6, 5, 1  // Right face
    };

    MeshAsset newMesh{};

    newMesh.meshBuffers = engine.uploadMesh(indices, vertices);
    newMesh.surfaces.push_back(GeoSurface{.startIndex = 0, .count = static_cast<uint32_t>(indices.size())});

    return newMesh;
}

void AssetManager::loadDefaultMeshes() {
    mMeshes["quad"] = createQuadMesh(mVkEngine);
    mMeshes["cube"] = createCubeMesh(mVkEngine);
}

void AssetManager::loadBrdfLut() {
    std::string path = mProjectPath + "/assets/skyboxes/brdf_lut.ktx2";
    fmt::println("Loading brdf lut");

    if (!std::filesystem::exists(path)) {
        fmt::println("Error: precomputed brdf lut not found");
        return;
    }

    ktxTexture2* texture;
    KTX_error_code result = ktxTexture2_CreateFromNamedFile(path.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

    if (result != KTX_SUCCESS) {
        fmt::println("Failed to load brdf lut from ktx2 file.");
        return;
    }

    VkSampler linear = mDefaultSamplers["linear"];
    AllocatedImage brdfLut{};

    brdfLut = mVkEngine.createImage(
        VkExtent3D{SkyboxAsset::BRDF_LUT_SIZE.width, SkyboxAsset::BRDF_LUT_SIZE.height, 1},
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false
    );

    uint32_t size = vkutil::getPixelSize(brdfLut.imageFormat) * brdfLut.imageExtent.width * brdfLut.imageExtent.height;
    AllocatedBuffer staging = mVkEngine.createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, brdfLut.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        ktx_size_t offset;
        ktxTexture2_GetImageOffset(texture, 0, 0, 0, &offset);
        void* data = static_cast<uint8_t*>(texture->pData) + offset;

        void* stagingData = mVkEngine.getDataFromBuffer(staging);
        memcpy(stagingData, data, size);

        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0; // Tightly packed
        copyRegion.bufferImageHeight = 0;

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        copyRegion.imageOffset = {0, 0, 0};
        copyRegion.imageExtent = {
            static_cast<uint32_t>(texture->baseWidth),
            static_cast<uint32_t>(texture->baseHeight),
            1
        };

        vkCmdCopyBufferToImage(
            commandBuffer,
            staging.buffer,
            brdfLut.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion
        );

        vkutil::transitionImage(commandBuffer, brdfLut.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }, true);

    mVkEngine.destroyBuffer(staging);
    ktxTexture2_Destroy(texture);

    mDefaultImages["brdflut"] = brdfLut;
}

void AssetManager::loadDefaultImages() {
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
	mDefaultImages["white"] = mVkEngine.createImage(
	    (void*)&white,
		VkExtent3D{ 1, 1, 1 },
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT
	);

	uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
	mDefaultImages["grey"] = mVkEngine.createImage(
	    (void*)&grey,
		VkExtent3D{ 1, 1, 1 },
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT
	);

	uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
	mDefaultImages["black"] = mVkEngine.createImage(
	    (void*)&black,
		VkExtent3D{ 1, 1, 1 },
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT
	);

    uint32_t normal = glm::packUnorm4x8(glm::vec4(0.5f, 0.5f, 1.0f, 0.0f));
    mDefaultImages["normal"] = mVkEngine.createImage(
        (void*)&normal,
        VkExtent3D{ 1, 1, 1 },
        VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT
    );

	// checkerboard image
	uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
	std::array<uint32_t, 16 *16 > pixels; // for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y*16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}

	mDefaultImages["error"] = mVkEngine.createImage(
	    pixels.data(),
		VkExtent3D{16, 16, 1},
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT
	);

	// default cubemaps
	AllocatedImage cubeMap = mVkEngine.createCubemap(
	    {1, 1},
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		glm::vec3(1.f, 0.f, 0.f)
	);

	mDefaultImages["red_cube"] = cubeMap;

	cubeMap = mVkEngine.createCubemap(
	    {1, 1},
		VK_FORMAT_R16G16B16A16_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		glm::vec3(0.f, 0.f, 0.f)
	);

	mDefaultImages["black_cube"] = cubeMap;
}

void AssetManager::loadDefaultSamplers() {
    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;
    sampl.anisotropyEnable = VK_TRUE;
    sampl.maxAnisotropy = mVkEngine.getMaxSamplerAnisotropy();
    sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampl.unnormalizedCoordinates = VK_FALSE;

    sampl.minLod = 0.0f;
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.mipLodBias = 0.0f;

    {
        VkSampler nearest;
        vkCreateSampler(mVkEngine.getDevice(), &sampl, nullptr, &nearest);
        mDefaultSamplers["nearest"] = nearest;
    }

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    {
        VkSampler linear;
        vkCreateSampler(mVkEngine.getDevice(), &sampl, nullptr, &linear);
        mDefaultSamplers["linear"] = linear;
    }
}

void AssetManager::freeResources() {
    for (auto& [k, v] : mImages) {
        mVkEngine.destroyImage(v);
    }

    for (auto& [k, v] : mMeshes) {
        mVkEngine.destroyBuffer(v.meshBuffers.indexBuffer);
        mVkEngine.destroyBuffer(v.meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : mSkyboxes) {
        mVkEngine.destroyImage(v.hdrImage);
        mVkEngine.destroyImage(v.envMap);
        mVkEngine.destroyImage(v.irrMap);
        mVkEngine.destroyImage(v.prefilteredEnvMap);
    }

    for (auto& [k, v] : mDefaultImages) {
        mVkEngine.destroyImage(v);
    }

    for (auto& [k, v] : mDefaultSamplers) {
        mVkEngine.destroySampler(v);
    }
}

void AssetManager::drawSkyboxAssets(ImGuiContext& context) {
    const uint32_t maxTextLength = 15;

    ImGui::PushFont(context.font12);
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    auto drawList = ImGui::GetWindowDrawList();
    auto& hdrIcon = mImguiImages["hdr-icon.png"];

    ImVec2 p0 = ImGui::GetCursorScreenPos();

    uint32_t itemsPerRow = 9;
    uint32_t row = 0;
    uint32_t col = 0;

    ImVec2 spacing = context.spacing;
    float iconWidth = (1000.f - (itemsPerRow + 1) * spacing.x) / itemsPerRow;
    ImVec2 iconSize = ImVec2(iconWidth, iconWidth);
    float textHeight = ImGui::CalcTextSize("dummy.hdr").y;

    for (auto& [k, v] : mSkyboxes) {
        ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y));
        ImVec2 max = ImVec2(min.x + iconSize.x, min.y + iconSize.y);

        std::string text = k;
        if (text.length() > maxTextLength) {
            text = text.substr(0, maxTextLength - 3) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float textOffset = (iconSize.x - textSize.x) * 0.5f;

        ImGui::SetCursorScreenPos(min);

        ImGui::BeginGroup();
        ImGui::Image(hdrIcon.id, iconSize);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - textHeight);
        ImGui::Text(text.c_str());
        ImGui::EndGroup();

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SKYBOX_PAYLOAD", k.c_str(), k.size() + 1);

            ImGui::Image(hdrIcon.id, iconSize);
            ImGui::Text(k.c_str());

            ImGui::EndDragDropSource();
        }

        if (++col == itemsPerRow) {
            row++;
            col = 0;
        }
    }

    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void AssetManager::drawSpriteAssets(ImGuiContext& context) {
    const uint32_t maxTextLength = 15;

    ImGui::PushFont(context.font12);
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    auto drawList = ImGui::GetWindowDrawList();
    auto& pngIcon = mImguiImages["png-icon.png"];
    auto& jpgIcon = mImguiImages["jpg-icon.png"];

    ImVec2 p0 = ImGui::GetCursorScreenPos();

    uint32_t itemsPerRow = 9;
    uint32_t row = 0;
    uint32_t col = 0;

    ImVec2 spacing = context.spacing;
    float iconWidth = (1000.f - (itemsPerRow + 1) * spacing.x) / itemsPerRow;
    ImVec2 iconSize = ImVec2(iconWidth, iconWidth);
    float textHeight = ImGui::CalcTextSize("dummy.jpg").y;

    for (auto& [k, v] : mImages) {
        ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y));
        ImVec2 max = ImVec2(min.x + iconSize.x, min.y + iconSize.y);

        std::string text = k;
        if (text.length() > maxTextLength) {
            text = text.substr(0, maxTextLength - 3) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float textOffset = (iconSize.x - textSize.x) * 0.5f;

        ImGui::SetCursorScreenPos(min);

        ImGui::BeginGroup();
        if (k.ends_with(".jpg"))
            ImGui::Image(jpgIcon.id, iconSize);
        else
            ImGui::Image(pngIcon.id, iconSize);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - textHeight);
        ImGui::Text(text.c_str());
        ImGui::EndGroup();

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SPRITE_PAYLOAD", k.c_str(), k.size() + 1);

            if (k.ends_with(".jpg"))
                ImGui::Image(jpgIcon.id, iconSize);
            else
                ImGui::Image(pngIcon.id, iconSize);

            ImGui::Text(k.c_str());

            ImGui::EndDragDropSource();
        }

        if (++col == itemsPerRow) {
            row++;
            col = 0;
        }
    }

    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void AssetManager::drawGltfAssets(ImGuiContext& context) {
    const uint32_t maxTextLength = 15;

    ImGui::PushFont(context.font12);
    ImGui::PushStyleColor(ImGuiCol_Text, WHITE);

    auto drawList = ImGui::GetWindowDrawList();
    auto& glbIcon = mImguiImages["glb-icon.png"];

    ImVec2 p0 = ImGui::GetCursorScreenPos();

    uint32_t itemsPerRow = 9;
    uint32_t row = 0;
    uint32_t col = 0;

    ImVec2 spacing = context.spacing;
    float iconWidth = (1000.f - (itemsPerRow + 1) * spacing.x) / itemsPerRow;
    ImVec2 iconSize = ImVec2(iconWidth, iconWidth);
    float textHeight = ImGui::CalcTextSize("dummy.glb").y;

    for (auto& [k, v] : mLoadedGltfs) {
        ImVec2 min = ImVec2(p0.x + col * (iconSize.x + spacing.x), p0.y + row * (iconSize.y + spacing.y));
        ImVec2 max = ImVec2(min.x + iconSize.x, min.y + iconSize.y);

        std::string text = k;
        if (text.length() > maxTextLength) {
            text = text.substr(0, maxTextLength - 3) + "...";
        }

        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        float textOffset = (iconSize.x - textSize.x) * 0.5f;

        ImGui::SetCursorScreenPos(min);

        ImGui::BeginGroup();
        ImGui::Image(glbIcon.id, iconSize);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - textHeight);
        ImGui::Text(text.c_str());
        ImGui::EndGroup();

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("GLTF_PAYLOAD", k.c_str(), k.size() + 1);

            ImGui::Image(glbIcon.id, iconSize);
            ImGui::Text(k.c_str());

            ImGui::EndDragDropSource();
        }

        if (++col == itemsPerRow) {
            row++;
            col = 0;
        }
    }

    ImGui::PopStyleColor();
    ImGui::PopFont();
}
