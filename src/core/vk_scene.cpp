#include "vk_scene.h"
#include "components/components.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include <imgui.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <memory>

Scene3D::Scene3D(const std::filesystem::path& path, VulkanEngine& vkEngine) : mVkEngine(vkEngine), mAssetManager(mVkEngine.getAssetManager()) {
    init();
    loadFromFile(path);

    mSceneData.sunlightColor = glm::vec4(1.0f);

    saveToFile();
}

Scene3D::~Scene3D() {
    freeResources();
}

void Scene3D::freeResources() {
    mDeletionQueue.flush();
}

void Scene3D::init() {
    // create scene data buffer
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mSceneDataUBOs[i].buffer = mVkEngine.createBuffer(
            sizeof(SceneData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
    }

    // add scene data buffer to deletion queue
    mDeletionQueue.push([&]() {
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mVkEngine.destroyBuffer(mSceneDataUBOs[i].buffer);
        }
    });

    mScriptManager = std::make_unique<ScriptManager>();

    mDeletionQueue.push([&]() {
        mEntities.clear();

        mScriptManager = nullptr;
    });
}

void Scene3D::loadFromFile(const std::filesystem::path& filePath) {
    mName = filePath.stem();

    // get json from file
    std::ifstream file(filePath);
    nlohmann::json sceneJson = nlohmann::json::parse(file);

    // load skybox
    if (sceneJson.contains("Skybox")) {
        mAssetManager.loadSkybox(sceneJson["Skybox"]);
        mSkybox = mAssetManager.getSkybox(sceneJson["Skybox"]);
    }

    // create scene data descriptor
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mSceneDataUBOs[i].globalDescriptorOffset = mVkEngine.getPipelineResourceManager().createSceneDescriptor(
            mSceneDataUBOs[i].buffer.deviceAddress,
            sizeof(SceneData),
            mSkybox ? mSkybox->irrMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
            mSkybox ? mSkybox->prefilteredEnvMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
            mSkybox ? mSkybox->brdfLut.imageView : mAssetManager.getDefaultImage("brdflut")->imageView,
            *mAssetManager.getSampler("linear")
        );
    }

    // load entities
    for (auto& entityJson : sceneJson["Entities"]) {
        auto componentJsons = entityJson["Components"];
        auto newEntity = createEntity(componentJsons["Metadata"]);

        // create components
        SerializableComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;
            auto typeName = getTypeName<T>();

            if (!componentJsons.contains(typeName) || typeName == "Metadata") {
                return;
            }

            fmt::println("Adding {} component to {}", std::string(typeName), std::string(componentJsons["Metadata"]["UUID"]));

            newEntity->addComponent<T>(componentFromJson<T>(entityJson["Components"][typeName]));
        });
    }

    // load and bind assets to entities
    {
        auto view = mRegistry.view<Sprite>();

        for (auto [entity, sprite] : view.each()) {
            mAssetManager.loadImage(sprite.name, VK_FORMAT_R8G8B8A8_SRGB);
            sprite.image = mAssetManager.getImage(std::filesystem::path(sprite.name).stem());
        }
    }

    {
        auto view = mRegistry.view<GLTF>();

        for (auto [entity, gltf] : view.each()) {
            mAssetManager.loadGltf(gltf.name);
            gltf.gltf = mAssetManager.getGltf(gltf.name);
        }
    }

    {
        auto view = mRegistry.view<Script, Metadata>();

        for (auto [entity, script, metadata] : view.each()) {
            mScriptManager->loadScript(script.name);
            mScriptManager->bindScript(script.name, *getEntity(metadata.uuid));
        }
    }

    // assign main camera
    {
        auto view = mRegistry.view<Transform, Camera, Metadata>();

        for (auto [entity, transform, camera, metadata] : view.each()) {
            if (camera.enabled) {
                mCameraEntity = getEntity(metadata.uuid);

                break;
            }
        }
    }

    // create hierarchy
    for (auto& entityJson : sceneJson["Entities"]) {
        std::string uuid = entityJson["Components"]["Metadata"]["UUID"];
        Entity* entity = getEntity(uuid);
        std::string parentUUID = entityJson["ParentUUID"];
        std::vector<std::string> childrenUUIDs = entityJson["ChildrenUUIDs"];

        if (parentUUID != "") {
            entity->setParent(getEntity(parentUUID));
        } else {
            mRootEntities.push_back(getEntity(uuid));
        }

        for (auto& child : childrenUUIDs) {
            entity->addChild(getEntity(child));
        }
    }

    fmt::println("Finished loading scene!");
}

void Scene3D::cleanupEntities() {
    // auto view = mRegistry.view<Destroy>();

    // std::vector<entt::entity> entities;

    // for (auto [entity] : view.each()) {
    //     std::string& uuid = mRegistry.get<UUIDComponent>(entity).uuid;

    //     mEntities.erase(uuid);
    // }
}

void Scene3D::propagateTransform() {
    for (auto& entity : mRootEntities) {
        entity->propagateTransform(glm::mat4(1.f));
    }
}

void Scene3D::render(RenderContext& renderContext) {
    renderContext = RenderContext{};

    {
        auto view = mRegistry.view<Transform, GLTF>();

        for (auto [entity, transform, gltf] : view.each()) {
            if (gltf.gltf == nullptr) {
                continue;
            }

            gltf.gltf->draw(transform.globalMatrix, renderContext);
        }
    }

    {
        auto view = mRegistry.view<Transform, Sprite>();

        for (auto [entity, transform, sprite] : view.each()) {
            if (sprite.image == nullptr) {
                continue;
            }

            renderContext.sprites.push_back(SpriteRenderObject{.image = sprite.image, .transform = transform.globalMatrix});
        }
    }

    renderContext.sceneData = mSceneData;
    renderContext.skybox = mSkybox;
}

void Scene3D::update(float deltaTime, const Input& input, int frameNumber) {
    // systems
    mScriptManager->update(deltaTime, input, mCameraEntity);

    // mScriptManager->onDestroy();
    cleanupEntities();

    mScriptManager->onInit(mRegistry);
    mScriptManager->onUpdate(mRegistry);
    propagateTransform();

    mScriptManager->onLateUpdate(mRegistry);
    propagateTransform();

    mSceneData.view = glm::mat4(1.f);
    mSceneData.projection = glm::mat4(1.f);

    if (mCameraEntity) {
        Camera& camera = mCameraEntity->getComponent<Camera>();
        Transform& cameraTransform = mCameraEntity->getComponent<Transform>();

        camera.aspectRatio = mVkEngine.getWindowAspectRatio();
        camera.updateMatrices(cameraTransform);

        mSceneData.viewPosition = glm::vec4(camera.globalPosition, 0.0f);
        mSceneData.view = camera.viewMatrix;
        mSceneData.projection = camera.projectionMatrix;
    }

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;

    mSceneData.ambientColor = glm::vec4(.1f);
    mSceneData.sunlightDirection = glm::vec4(.2f, 1.0f, .5f, 1.f);
    mSceneData.sunlightColor = glm::vec4(0.f);


    *(SceneData*)mSceneDataUBOs[frameNumber].buffer.allocInfo.pMappedData = mSceneData;
}

Entity* Scene3D::createEntity(const nlohmann::json& metaJson) {
    return createEntity(componentFromJson<Metadata>(metaJson));
}

Entity* Scene3D::createEntity(Metadata meta) {
    mEntities[meta.uuid] = std::make_unique<Entity>(meta, mRegistry);

    return mEntities[meta.uuid].get();
}

Entity* Scene3D::createEntity() {
    Metadata meta;
    meta.uuid = "Entity_" + std::to_string(mNextEntityID++);

    return createEntity(meta);
}

Entity* Scene3D::getEntity(const std::string& name) {
    if (!mEntities.contains(name)) {
        return nullptr;
    }

    return mEntities[name].get();
}

void Scene3D::destroyEntity(const std::string& name) {
    mEntities.erase(name);
}

void Scene3D::drawGui() {
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // camera
        // mCamera.drawGui();
        if (ImGui::TreeNode("Camera")) {
            ImGui::InputFloat3("position", (float*)&mCameraEntity->getComponent<Transform>().position);
            ImGui::InputFloat3("rotation", (float*)&mCameraEntity->getComponent<Transform>().rotation);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Hierarchy")) {
            for (auto& entity : mRootEntities) {
                entity->drawGUI();
            }

            if (ImGui::Button("Add new entity")) {
                Entity* newEntity = createEntity();
                mRootEntities.push_back(newEntity);
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

void Scene3D::setGlobalDescriptorOffset(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, int frameNumber) {
    uint32_t bufferIndexUbo = 0;

    vkCmdSetDescriptorBufferOffsetsEXT(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &bufferIndexUbo, &mSceneDataUBOs[frameNumber].globalDescriptorOffset);
}

nlohmann::json Scene3D::toJson() {
    nlohmann::json sceneJson;

    std::vector<nlohmann::json> entityJsons;

    for (auto& [UUID, entity] : mEntities) {
        entityJsons.push_back(entity->toJson());
    }

    sceneJson["Entities"] = entityJsons;

    if (mSkybox) {
        sceneJson["Skybox"] = mSkybox->name;
    }

    return sceneJson;
}

void Scene3D::saveToFile() {
    std::filesystem::path filePath = "assets/scenes/1" + mName + ".json";

    std::ofstream outFile(filePath);

    outFile << toJson().dump(2);

    outFile.close();
}
