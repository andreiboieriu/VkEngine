#include "skybox.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/trigonometric.hpp"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include "stb_image.h"
#include "vk_initializers.h"
#include <imgui.h>

Skybox::Skybox(VulkanEngine& vkEngine) : mVkEngine(vkEngine) {
    createPipelineLayout();
    createPipeline();
    createCubeMesh();

    // load("assets/skybox/sunset_sky.hdr");
    load("assets/skybox/jupiter.hdr");
    // load("assets/skybox/nebula-1.hdr");
}

Skybox::~Skybox() {
    freeResources();
}

void Skybox::createPipelineLayout() {
    DescriptorLayoutBuilder builder;
    mDescriptorSetLayout = builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                      .build(mVkEngine.getDevice(), nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    std::vector<VkDescriptorSetLayout> setLayouts = {
        mDescriptorSetLayout
    };

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkPushConstantRange> pushRanges = {
        pushConstantRange
    };

    mPipelineLayout = vkutil::createPipelineLayout(setLayouts, pushRanges, mVkEngine.getDevice());
}

void Skybox::createPipeline() {
    VkShaderModule vertShader;

    if (!vkutil::loadShaderModule("shaders/skybox.vert.spv", mVkEngine.getDevice(), &vertShader)) {
        fmt::println("Error when building shader: shaders/skybox.vert.spv");
    }

    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/skybox.frag.spv", mVkEngine.getDevice(), &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    PipelineBuilder builder;

    mPipeline = builder.setShaders(vertShader, fragShader)
                        .setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
                        .setPolygonMode(VK_POLYGON_MODE_FILL)
                        .setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                        .setMultisampling()
                        .disableBlending()
                        .enableDepthTesting(true, VK_COMPARE_OP_GREATER_OR_EQUAL)
                        .setColorAttachmentFormat(mVkEngine.getDrawImageFormat())
                        .setDepthFormat(mVkEngine.getDepthImageFormat())
                        .setLayout(mPipelineLayout)
                        .buildPipeline(mVkEngine.getDevice(), 0);

    vkDestroyShaderModule(mVkEngine.getDevice(), fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_equi_to_cube.frag.spv", mVkEngine.getDevice(), &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    mEquiToCubePipeline = builder.buildPipeline(mVkEngine.getDevice(), 0);

    vkDestroyShaderModule(mVkEngine.getDevice(), fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_env_to_irradiance.frag.spv", mVkEngine.getDevice(), &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    mEnvToIrrPipeline = builder.buildPipeline(mVkEngine.getDevice(), 0);

    vkDestroyShaderModule(mVkEngine.getDevice(), fragShader, nullptr);

    if (!vkutil::loadShaderModule("shaders/skybox_prefilter_env.frag.spv", mVkEngine.getDevice(), &fragShader)) {
        fmt::println("Error when building shader: shaders/skybox.frag.spv");
    }

    builder.setShaders(vertShader, fragShader);
    mPrefilterEnvPipeline = builder.buildPipeline(mVkEngine.getDevice(), 0);

    vkDestroyShaderModule(mVkEngine.getDevice(), vertShader, nullptr);
    vkDestroyShaderModule(mVkEngine.getDevice(), fragShader, nullptr);
}

void Skybox::createCubeMesh() {
    mCubeMesh.name = "skyboxCube";

    std::vector<Vertex> cubeVertices = {
        Vertex{.position = glm::vec3(-1.0f, -1.0f, -1.0f)},
        Vertex{.position = glm::vec3(1.0f, -1.0f, -1.0f)},
        Vertex{.position = glm::vec3(1.0f, 1.0f, -1.0f)},
        Vertex{.position = glm::vec3(-1.0f, 1.0f, -1.0f)},
        Vertex{.position = glm::vec3(-1.0f, -1.0f, 1.0f)},
        Vertex{.position = glm::vec3(1.0f, -1.0f, 1.0f)},
        Vertex{.position = glm::vec3(1.0f, 1.0f, 1.0f)},
        Vertex{.position = glm::vec3(-1.0f, 1.0f, 1.0f)}
    };

    std::vector<uint32_t> cubeIndices = {
        0, 1, 2, 2, 3, 0, // Front face
        4, 5, 6, 6, 7, 4, // Back face
        0, 1, 5, 5, 4, 0, // Bottom face
        2, 3, 7, 7, 6, 2, // Top face
        0, 3, 7, 7, 4, 0, // Left face
        1, 2, 6, 6, 5, 1  // Right face
    };

    mCubeMesh.surfaces.push_back(GeoSurface{.startIndex = 0, .count = static_cast<uint32_t>(cubeIndices.size())});

    mCubeMesh.meshBuffers = mVkEngine.uploadMesh(cubeIndices, cubeVertices);
    mPushConstants.vertexBuffer = mCubeMesh.meshBuffers.vertexBufferAddress;
}

void Skybox::freeResources() {
    mVkEngine.destroyBuffer(mCubeMesh.meshBuffers.indexBuffer);
    mVkEngine.destroyBuffer(mCubeMesh.meshBuffers.vertexBuffer);

    vkDestroyDescriptorSetLayout(mVkEngine.getDevice(), mDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(mVkEngine.getDevice(), mPipelineLayout, nullptr);
    vkDestroyPipeline(mVkEngine.getDevice(), mPipeline, nullptr);
    vkDestroyPipeline(mVkEngine.getDevice(), mEquiToCubePipeline, nullptr);
    vkDestroyPipeline(mVkEngine.getDevice(), mEnvToIrrPipeline, nullptr);
    vkDestroyPipeline(mVkEngine.getDevice(), mPrefilterEnvPipeline, nullptr);

    mVkEngine.destroyImage(mHDRImage);
    mVkEngine.destroyImage(mEnvMap);
    mVkEngine.destroyImage(mIrrMap);
    mVkEngine.destroyImage(mPrefilteredEnvMap);
    mVkEngine.destroyImage(mBrdfLut);
}

void Skybox::draw(VkCommandBuffer commandBuffer) {
    // if (mChosenSkybox == "empty") {
    //     return;
    // }

    // bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

    // bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, mCubeMesh.meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // push descriptor set
    DescriptorWriter writer;
    std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, mEnvMap.imageView, mVkEngine.getDefaultLinearSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                               .getWrites();

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

    // push constants
    vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &mPushConstants);

    // draw
    vkCmdDrawIndexed(commandBuffer, mCubeMesh.surfaces[0].count, 1, mCubeMesh.surfaces[0].startIndex, 0, 0);
}

void Skybox::drawGui() {
    if (ImGui::TreeNode("Skybox")) {
        // if (ImGui::BeginPopupContextItem("select skybox popup")) {
        //     if (ImGui::Selectable("nebula")) {
        //         mChosenSkybox = "nebula";
        //         mHDRImage = mVkEngine.getResourceManager().getSkyboxCubemap("nebula");
        //     }

        //     if (ImGui::Selectable("anime")) {
        //         mChosenSkybox = "anime";
        //         mHDRImage = mVkEngine.getResourceManager().getSkyboxCubemap("anime");
        //     }

        //     ImGui::EndPopup();
        // }

        // if (ImGui::Button(mChosenSkybox.c_str())) {
        //     ImGui::OpenPopup("select skybox popup");
        // }

        ImGui::SliderInt("mip level", &mMipLevel, 0, mPrefilteredEnvMap.mipLevels - 1);

        ImGui::TreePop();
    }
}

void Skybox::renderToCubemap(AllocatedImage& src, AllocatedImage& dest, VkPipeline pipeline, VkExtent2D destSize, uint32_t mipLevel, bool flipViewport) {
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

    fmt::println("dest size: {} {}, mip: {}", destSize.width, destSize.height, mipLevel);

    // create push constants
    PushConstants captureConstants{};
    captureConstants.vertexBuffer = mCubeMesh.meshBuffers.vertexBufferAddress;
    captureConstants.mipLevel = mipLevel;
    captureConstants.totalMips = dest.mipLevels;

    // transition cubemap to color attachment layout
    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, dest.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    });

    for (uint32_t i = 0; i < 6; i++) {
        captureConstants.projectionView = captureProjection * glm::mat4(glm::mat3(captureViews[i]));

        // create cubemap face view
        VkImageView cubeFaceView;

        VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(VK_FORMAT_R16G16B16A16_SFLOAT, dest.image, VK_IMAGE_ASPECT_COLOR_BIT);
        viewInfo.subresourceRange.baseArrayLayer = i;
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


            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            // bind index buffer
            vkCmdBindIndexBuffer(commandBuffer, mCubeMesh.meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

            // push descriptor set
            DescriptorWriter writer;
            std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, src.imageView, mVkEngine.getDefaultLinearSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                                        .getWrites();

            vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

            // push constants
            vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &captureConstants);

            // draw
            vkCmdDrawIndexed(commandBuffer, mCubeMesh.surfaces[0].count, 1, mCubeMesh.surfaces[0].startIndex, 0, 0);

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

void Skybox::renderBrdfLut() {
    ComputeEffect computeEffect("skybox_brdf_lut", mVkEngine);

    mVkEngine.immediateSubmit([&](VkCommandBuffer commandBuffer) {
        vkutil::transitionImage(commandBuffer, mBrdfLut.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        computeEffect.execute(commandBuffer, mBrdfLut, BRDF_LUT_SIZE, true);
        vkutil::transitionImage(commandBuffer, mBrdfLut.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });
}

void Skybox::load(std::filesystem::path filePath) {
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

    // create hdr image
    mHDRImage = mVkEngine.createImage(
        data,
        VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT,
        false
    );

    // create environment map
    mEnvMap = mVkEngine.createEmptyCubemap(
        ENV_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );

    renderToCubemap(mHDRImage, mEnvMap, mEquiToCubePipeline, ENV_MAP_SIZE, 0, true);

    // create irradiance cube map
    mIrrMap = mVkEngine.createEmptyCubemap(
        IRR_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    );

    renderToCubemap(mEnvMap, mIrrMap, mEnvToIrrPipeline, IRR_MAP_SIZE);

    // create prefiltered environment map
    mPrefilteredEnvMap = mVkEngine.createEmptyCubemap(
        PREFILTERED_ENV_MAP_SIZE,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        true
    );

    for (uint32_t mip = 0; mip < mPrefilteredEnvMap.mipLevels; mip++) {
        renderToCubemap(mEnvMap, mPrefilteredEnvMap, mPrefilterEnvPipeline, PREFILTERED_ENV_MAP_SIZE, mip);
    }

    // create brdf lut image
    mBrdfLut = mVkEngine.createImage(
        VkExtent3D{BRDF_LUT_SIZE.width, BRDF_LUT_SIZE.height, 1},
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
        false
    );

    renderBrdfLut();
}
