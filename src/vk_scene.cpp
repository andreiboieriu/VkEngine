#include "vk_scene.h"
#include "ecs_components/components.h"
#include "ecs_systems/systems.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include <imgui.h>

#include <memory>

Scene3D::Scene3D(const std::string& name) : mName(name) {
    init();

    mSceneData.sunlightColor = glm::vec4(1.0f);

    // Entity& station = createEntity(UUID("station"));
    // station.addComponent<Transform>();
    // station.addComponent<GLTF>();

    // station.getComponent<Transform>().rotation = glm::radians(glm::vec3(0.f, 230.f, 0.f));

    // station.bindGLTF("sci_fi_hangar");

    // Entity& spaceship = createEntity(UUID("fighter_spaceship"));
    // spaceship.addComponent<Transform>();
    // spaceship.addComponent<GLTF>();
    // spaceship.addComponent<Script>();
    // spaceship.addComponent<SphereCollider>();

    Entity& asteroid = createEntity(UUID("fighter_spaceship"));
    asteroid.addComponent<Transform>();
    asteroid.addComponent<GLTF>();
    asteroid.addComponent<Script>();

    asteroid.getComponent<Transform>().position = glm::vec3(0.f, 0.f, -2.0f);

    asteroid.bindGLTF("fighter_spaceship");

    mScriptManager->loadScript("scripts/script.lua");
    mScriptManager->bindScript("scripts/script.lua", asteroid);

    Entity& mainCamera = createEntity(UUID("main_camera"));
    mainCamera.addComponent<Transform>();
    mainCamera.addComponent<Camera>();
    mainCamera.addComponent<Script>();

    // mainCamera.bindScript("CameraScript");


    // asteroidSpawner.bindScript("AsteroidSpawnerScript");

    mRootEntityUUIDs.push_back(asteroid.getUUID());
    // mRootEntityUUIDs.push_back(spaceship.mUUID);
    mRootEntityUUIDs.push_back(mainCamera.getUUID());

    mMainCameraHolder = mainCamera.getUUID();

    saveToFile();

    // scriptSystemStart(mRegistry);
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

    mSkybox = std::make_unique<Skybox>();
    mScriptManager = std::make_unique<ScriptManager>(mRegistry);

    mDeletionQueue.push([&]() {
        mSkybox = nullptr;
        mScriptManager = nullptr;
    });

    // create scene data descriptor
    mGlobalDescriptorOffset = VulkanEngine::get().getDescriptorManager().createSceneDescriptor(
        mSceneDataBuffer.deviceAddress,
        sizeof(SceneData),
        mSkybox->getIrradianceMap().imageView,
        mSkybox->getPrefilteredEnvMap().imageView,
        mSkybox->getBrdfLut().imageView
    );
}

void Scene3D::cleanupEntities() {
    auto view = mRegistry.view<Destroy>();

    for (auto [entity] : view.each()) {
        UUID& uuid = mRegistry.get<UUIDComponent>(entity).uuid;

        mEntityMap.erase(uuid);
    }
}

void Scene3D::propagateTransform() {
    for (auto& entityUUID : mRootEntityUUIDs) {
        mEntityMap[entityUUID]->propagateTransform(glm::mat4(1.f));
    }
}

void Scene3D::render(RenderContext& renderContext) {
    auto view = mRegistry.view<Transform, GLTF>();

    for (auto [entity, transform, gltf] : view.each()) {
        if (gltf.gltf == nullptr) {
            continue;
        }

        gltf.gltf->draw(transform.globalMatrix, renderContext);
    }
}

void Scene3D::update() {
    // systems
    mScriptManager->update();

    // mScriptManager->onDestroy();
    cleanupEntities();

    mScriptManager->onUpdate();

    propagateTransform();

    Camera& camera = mEntityMap[mMainCameraHolder]->getComponent<Camera>();
    Transform& cameraTransform = mEntityMap[mMainCameraHolder]->getComponent<Transform>();

    camera.aspectRatio = VulkanEngine::get().getWindowAspectRatio();
    camera.updateMatrices(cameraTransform);

    mSceneData.view = camera.viewMatrix;
    mSceneData.projection = camera.projectionMatrix;

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;

    mSceneData.ambientColor = glm::vec4(.1f);
    // mSceneData.sunlightColor = glm::vec4(1.f, 1.f, 1.f, 1.0f);
    mSceneData.sunlightDirection = glm::vec4(.2f, 1.0f, .5f, 1.f);

    mSceneData.viewPosition = glm::vec4(camera.globalPosition, 0.0f);

    *(SceneData*)mSceneDataBuffer.allocInfo.pMappedData = mSceneData;

    if (mSkybox != nullptr)
        mSkybox->update(mSceneData.projection * glm::mat4(glm::mat3(mSceneData.view)));
        // mSkybox->update(mSceneData.projection * mSceneData.view);
}

Entity& Scene3D::createEntity(const UUID& name) {
    mEntityMap[name] = std::make_unique<Entity>(name, mRegistry);

    return *mEntityMap[name];
}

Entity& Scene3D::getEntity(const UUID& name) {
    return *mEntityMap[name];
}

void Scene3D::destroyEntity(const UUID& name) {
    mEntityMap.erase(name);
}

void Scene3D::drawGui() {
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // camera
        // mCamera.drawGui();
        if (ImGui::TreeNode("Camera")) {
            ImGui::InputFloat3("position", (float*)&mEntityMap[mMainCameraHolder]->getComponent<Transform>().position);
            ImGui::InputFloat3("rotation", (float*)&mEntityMap[mMainCameraHolder]->getComponent<Transform>().rotation);

            ImGui::TreePop();
        }

        // skybox
        mSkybox->drawGui();

        if (ImGui::TreeNode("Hierarchy")) {
            for (auto& entityUUID : mRootEntityUUIDs) {
                mEntityMap[entityUUID]->drawGUI();
            }

            if (ImGui::Button("Add new entity")) {
                Entity& newEntity = createEntity();
                mRootEntityUUIDs.push_back(newEntity.getUUID());
            }

            ImGui::TreePop();
        }

        ImGui::InputFloat("light color", &mSceneData.sunlightColor.x);
        mSceneData.sunlightColor.y = mSceneData.sunlightColor.x;
        mSceneData.sunlightColor.z = mSceneData.sunlightColor.x;

        if (ImGui::Button("Save to file")) {
            saveToFile();
        }
    }

    auto view = mRegistry.view<Script>();

    for (auto [entity, scriptable] : view.each()) {
        // if (scriptable.script)
        //     scriptable.script->onDrawGUI();
    }

    ImGui::End();
}

void Scene3D::setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
    uint32_t bufferIndexUbo = 0;

    vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bufferIndexUbo, &mGlobalDescriptorOffset);
}

nlohmann::json Scene3D::toJson() {
    nlohmann::json sceneJson;

    sceneJson["Name"] = mName;

    std::vector<nlohmann::json> entityJsons;

    for (auto& [UUID, entity] : mEntityMap) {
        entityJsons.push_back(entity->toJson());
    }

    sceneJson["Entities"] = entityJsons;

    return sceneJson;
}

void Scene3D::saveToFile() {
    std::filesystem::path filePath = "assets/scenes/" + mName + ".json";

    std::ofstream outFile(filePath);

    outFile << toJson().dump(4);

    outFile.close();
}

void Scene3D::loadFromFile(std::filesystem::path filePath) {
    std::ifstream file(filePath);

    nlohmann::json sceneJson = nlohmann::json::parse(file);

    mName = sceneJson["Name"];

    // for (auto& entityJson : sceneJson["Entities"]) {
    //     mTopNodes.push_back(std::make_shared<SceneNode>(nodeJson, entt::null));
    // }
}

const glm::mat4 Scene3D::getViewProj() {
    return mSceneData.viewProjection;
}
