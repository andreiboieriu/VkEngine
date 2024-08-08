#include "skybox.h"

#include "vk_descriptors.h"
#include "vk_engine.h"
#include "vk_pipelines.h"
#include "stb_image.h"
#include "vk_initializers.h"
#include "imgui.h"

Skybox::Skybox() {
    createPipelineLayout();
    createPipeline();
    createCubeMesh();
}

Skybox::~Skybox() {
    freeResources();
}

void Skybox::createPipelineLayout() {
    DescriptorLayoutBuilder builder;
    mDescriptorSetLayout = builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                      .build(VulkanEngine::get().getDevice(), nullptr, VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

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

    mPipelineLayout = vkutil::createPipelineLayout(setLayouts, pushRanges);
}

void Skybox::createPipeline() {
    VkShaderModule vertShader;

    if (!vkutil::loadShaderModule("shaders/skybox.vert.spv", VulkanEngine::get().getDevice(), &vertShader)) {
        fmt::println("Error when building shader: shaders/skybox.vert.spv");
    }

    VkShaderModule fragShader;

    if (!vkutil::loadShaderModule("shaders/skybox.frag.spv", VulkanEngine::get().getDevice(), &fragShader)) {
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
                        .setColorAttachmentFormat(VulkanEngine::get().getDrawImageFormat())
                        .setDepthFormat(VulkanEngine::get().getDepthImageFormat())
                        .setLayout(mPipelineLayout)
                        .buildPipeline(VulkanEngine::get().getDevice(), 0);

    vkDestroyShaderModule(VulkanEngine::get().getDevice(), vertShader, nullptr);
    vkDestroyShaderModule(VulkanEngine::get().getDevice(), fragShader, nullptr);
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

    mCubeMesh.meshBuffers = VulkanEngine::get().uploadMesh(cubeIndices, cubeVertices);
    mPushConstants.vertexBuffer = mCubeMesh.meshBuffers.vertexBufferAddress;
}

void Skybox::freeResources() {
    VulkanEngine::get().destroyBuffer(mCubeMesh.meshBuffers.indexBuffer);
    VulkanEngine::get().destroyBuffer(mCubeMesh.meshBuffers.vertexBuffer);

    vkDestroyDescriptorSetLayout(VulkanEngine::get().getDevice(), mDescriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(VulkanEngine::get().getDevice(), mPipelineLayout, nullptr);
    vkDestroyPipeline(VulkanEngine::get().getDevice(), mPipeline, nullptr);
}

void Skybox::draw(VkCommandBuffer commandBuffer) {
    if (mChosenSkybox == "empty") {
        return;
    }

    // bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    
    // bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, mCubeMesh.meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    // push descriptor set
    DescriptorWriter writer;
    std::vector<VkWriteDescriptorSet> descriptorWrites = writer.writeImage(0, mTexture.imageView, VulkanEngine::get().getDefaultLinearSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                                                               .getWrites();

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, (uint32_t)descriptorWrites.size(), descriptorWrites.data());

    // push constants
    vkCmdPushConstants(commandBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &mPushConstants);

    // draw
    vkCmdDrawIndexed(commandBuffer, mCubeMesh.surfaces[0].count, 1, mCubeMesh.surfaces[0].startIndex, 0, 0);
}

void Skybox::drawGui() {
    if (ImGui::TreeNode("Skybox")) {
        if (ImGui::BeginPopupContextItem("select skybox popup")) {
            if (ImGui::Selectable("nebula")) {
                mChosenSkybox = "nebula";
                mTexture = VulkanEngine::get().getResourceManager().getSkyboxCubemap("nebula");
            }

            if (ImGui::Selectable("anime")) {
                mChosenSkybox = "anime";
                mTexture = VulkanEngine::get().getResourceManager().getSkyboxCubemap("anime");
            }

            ImGui::EndPopup();
        }

        if (ImGui::Button(mChosenSkybox.c_str())) {
            ImGui::OpenPopup("select skybox popup");
        }

        ImGui::TreePop();
    }
}