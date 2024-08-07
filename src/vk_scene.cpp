#include "vk_scene.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include "glm/gtx/transform.hpp"

#include <memory>

Scene3D::Scene3D(const std::string& name) : mName(name) {
    init();
}
    
Scene3D::~Scene3D() {
    freeResources();
}

void Scene3D::freeResources() {
    mDeletionQueue.flush();
}

void Scene3D::init() {
    // create scene data buffer
    mSceneDataBuffer = VulkanEngine::get().createBuffer(
        sizeof(SceneData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
        );

    // add scene data buffer to deletion queue
    mDeletionQueue.push([&]() {
        VulkanEngine::get().destroyBuffer(mSceneDataBuffer);
    });

    // create skybox
    mSkybox = std::make_unique<Skybox>("assets/skybox/nebula/");

    mDeletionQueue.push([&]() {
        mSkybox = nullptr;
    });

    // create scene data descriptor
    mGlobalDescriptorOffset = VulkanEngine::get().getDescriptorManager().createSceneDescriptor(mSceneDataBuffer.deviceAddress, sizeof(SceneData));

    mTestGLTF = std::make_shared<LoadedGLTF>("assets/hull_spaceship.glb");

    mDeletionQueue.push([&]() {
        mTestGLTF->freeResources();
    });

}

void Scene3D::update(float dt, float aspectRatio, const UserInput& userInput) {
    mCamera.update(dt, aspectRatio, userInput);

    mSceneData.view = mCamera.getViewMatrix();
    mSceneData.projection = mCamera.getProjectionMatrix();

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;

    mSceneData.ambientColor = glm::vec4(.1f);
    mSceneData.sunlightColor = glm::vec4(1.f, 1.f, 1.f, 1.0f);
    mSceneData.sunlightDirection = glm::vec4(0.f, 1.0f, .5f, 1.f);

    mSceneData.viewPosition = glm::vec4(mCamera.getPosition(), 0.0f);

    *(SceneData*)mSceneDataBuffer.allocInfo.pMappedData = mSceneData;

    mSkybox->update(mSceneData.projection * glm::mat4(glm::mat3(mSceneData.view)));
}

void Scene3D::draw(RenderContext &context) {
    mTestGLTF->draw(glm::scale(glm::vec3(5.f)), context);
}

void Scene3D::setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
    uint32_t bufferIndexUbo = 0;

    vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bufferIndexUbo, &mGlobalDescriptorOffset);
}
