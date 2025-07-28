#include "vk_scene.h"
#include "components/components.h"
#include "vk_descriptors.h"
#include "vk_engine.h"
#include <imgui.h>

#include <glm/gtx/matrix_decompose.hpp>
#include <memory>
#include <entt.hpp>

Scene3D::Scene3D(std::filesystem::path path, VulkanEngine& vkEngine) : mVkEngine(vkEngine), mAssetManager(mVkEngine.getAssetManager()), mScriptManager(mVkEngine.getScriptManager()) {
    init();
    loadFromFile(path);
}

Scene3D::Scene3D(VulkanEngine& vkEngine) : mVkEngine(vkEngine), mAssetManager(mVkEngine.getAssetManager()), mScriptManager(mVkEngine.getScriptManager()) {
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
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mSceneDataUBOs[i].buffer = mVkEngine.createBuffer(
            sizeof(SceneData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
    }

    // create scene data descriptors
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mSceneDataUBOs[i].globalDescriptorOffset = mVkEngine.getPipelineResourceManager().createSceneDescriptor(
            mSceneDataUBOs[i].buffer.deviceAddress,
            sizeof(SceneData),
            mAssetManager.getDefaultImage("black_cube")->imageView,
            mAssetManager.getDefaultImage("black_cube")->imageView,
            mAssetManager.getDefaultImage("brdflut")->imageView,
            *mAssetManager.getSampler("linear")
        );
    }

    // add scene data buffer to deletion queue
    mDeletionQueue.push([&]() {
        for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            mVkEngine.destroyBuffer(mSceneDataUBOs[i].buffer);
        }
    });

    mDeletionQueue.push([&]() {
        mEntities.clear();
    });

    mSceneData.sunlightColor = glm::vec4(1.0f);
    mSceneData.sunlightDirection = glm::vec4(0.f);
    mSceneData.clearColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.2f);
    mSceneData.cascadePlanes = glm::vec4(20.f, 55.f, 112.f, 0.f);
    mSceneData.shadowConfig = glm::vec4(0.5f, 1.0f, 0.2f, 0.f);
}

void Scene3D::loadFromFile(std::filesystem::path filePath) {
    mSkybox = nullptr;
    mRootEntities.clear();
    mEntities.clear();
    mRegistry.clear();

    // get json from file
    // fmt::println("Loading scene from json file: {}", filePath.c_str());
    std::ifstream file(filePath);
    nlohmann::json sceneJson = nlohmann::json::parse(file);

    // load skybox
    if (sceneJson.contains("Skybox")) {
        mAssetManager.loadSkybox(sceneJson["Skybox"]);
        mSkybox = mAssetManager.getSkybox(sceneJson["Skybox"]);
    }

    if (mSkybox) {
        mSceneData.sunlightDirection = glm::vec4(mSkybox->defaultDirectionalLight, 1.f);
    }

    // create scene data descriptor
    for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        mVkEngine.getPipelineResourceManager().updateSceneDescriptor(
            mSceneDataUBOs[i].globalDescriptorOffset,
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

            // fmt::println("Adding {} component to {}", std::string(typeName), std::string(componentJsons["Metadata"]["UUID"]));

            newEntity->addComponent<T>(componentFromJson<T>(entityJson["Components"][typeName]));
        });
    }

    // load and bind assets to entities
    {
        auto view = mRegistry.view<Sprite>();

        for (auto [entity, sprite] : view.each()) {
            if (sprite.name == "None" || sprite.name == "") continue;

            mAssetManager.loadImage(sprite.name, VK_FORMAT_R8G8B8A8_SRGB);
            sprite.image = mAssetManager.getImage(sprite.name);
        }
    }

    {
        auto view = mRegistry.view<GLTF>();

        for (auto [entity, gltf] : view.each()) {
            if (gltf.name == "None" || gltf.name == "") continue;

            mAssetManager.loadGltf(gltf.name);
            gltf.gltf = mAssetManager.getGltf(gltf.name);
        }
    }

    {
        auto view = mRegistry.view<Script, Metadata>();

        for (auto [entity, script, metadata] : view.each()) {
            if (script.name == "None" || script.name == "") continue;

            mScriptManager.loadScript(script.name);
            mScriptManager.bindScript(script.name, *getEntity(metadata.uuid));
        }
    }

    // assign main camera
    {
        auto view = mRegistry.view<Camera, Metadata>();

        for (auto [entity, camera, metadata] : view.each()) {
            mCameraEntity = getEntity(metadata.uuid);
            break;
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

    if (sceneJson.contains("Config")) {
        auto lightDirection = sceneJson["Config"]["lightDirection"];
        mSceneData.sunlightDirection.x = lightDirection[0];
        mSceneData.sunlightDirection.y = lightDirection[1];
        mSceneData.sunlightDirection.z = lightDirection[2];

        auto clearColor = sceneJson["Config"]["clearColor"];
        mSceneData.clearColor.x = clearColor[0];
        mSceneData.clearColor.y = clearColor[1];
        mSceneData.clearColor.z = clearColor[2];

        auto lightColor = sceneJson["Config"]["lightColor"];
        mSceneData.sunlightColor.x = lightColor[0];
        mSceneData.sunlightColor.y = lightColor[1];
        mSceneData.sunlightColor.z = lightColor[2];

        if (sceneJson["Config"].contains("cascadePlanes")) {
            auto cascadePlanes = sceneJson["Config"]["cascadePlanes"];
            mSceneData.cascadePlanes.x = cascadePlanes[0];
            mSceneData.cascadePlanes.y = cascadePlanes[1];
            mSceneData.cascadePlanes.z = cascadePlanes[2];
        }

        if (sceneJson["Config"].contains("shadowConfig")) {
            auto shadowConfig = sceneJson["Config"]["shadowConfig"];
            mSceneData.shadowConfig.x = shadowConfig[0];
            mSceneData.shadowConfig.y = shadowConfig[1];
            mSceneData.shadowConfig.z = shadowConfig[2];
        }
    }

    propagateTransform();
    initSceneDataBuffers();
}

void Scene3D::cleanupEntities() {
    auto view = mRegistry.view<Destroy, Metadata>();

    std::vector<entt::entity> entities;

    for (auto [entity, metadata] : view.each()) {
        std::string& uuid = metadata.uuid;
        auto newEnd = std::remove(mRootEntities.begin(), mRootEntities.end(), getEntity(metadata.uuid));
        mRootEntities.erase(newEnd, mRootEntities.end());

        mEntities.erase(uuid);
    }
}

void Scene3D::propagateTransform() {
    for (auto& entity : mRootEntities) {
        entity->propagateTransform(glm::mat4(1.f), mVkEngine.getDrawExtent());
    }
}

void Scene3D::render(RenderContext& renderContext) {
    renderContext = RenderContext{};
    renderContext.fov = mCameraFov;
    renderContext.near = mNear;
    renderContext.far = mFar;
    renderContext.sceneData = mSceneData;
    renderContext.skybox = mSkybox;

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
}

void Scene3D::initSceneDataBuffers() {
    mSceneData.view = glm::mat4(1.f);
    mSceneData.projection = glm::mat4(1.f);

    if (mCameraEntity && mCameraEntity->hasComponent<Transform>()) {
        Camera& camera = mCameraEntity->getComponent<Camera>();
        Transform& cameraTransform = mCameraEntity->getComponent<Transform>();

        camera.aspectRatio = mVkEngine.getWindowAspectRatio();
        camera.updateMatrices(cameraTransform);

        mSceneData.viewPosition = glm::vec4(camera.globalPosition, 0.0f);
        mSceneData.view = camera.viewMatrix;
        mSceneData.projection = camera.projectionMatrix;
    }

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;

    *(SceneData*)mSceneDataUBOs[0].buffer.allocInfo.pMappedData = mSceneData;
    *(SceneData*)mSceneDataUBOs[1].buffer.allocInfo.pMappedData = mSceneData;
}

void Scene3D::computeSunScreenPos() {
    float distance = mFar - 2.0;
    glm::vec3 sunWorldPos = mSceneData.viewPosition + mSceneData.sunlightDirection * distance;

    glm::vec4 lightClipPos = mSceneData.viewProjection * glm::vec4(sunWorldPos, 1.0f);
    glm::vec3 lightNDC = glm::vec3(lightClipPos) / lightClipPos.w;

    bool inView = lightNDC.x >= -1.2f && lightNDC.x <= 1.2f &&
                  lightNDC.y >= -1.2f && lightNDC.y <= 1.2f &&
                  lightClipPos.z >= 0.0f && lightClipPos.z <= lightClipPos.w;

    glm::vec2 drawExtent = mVkEngine.getDrawExtent();
    glm::vec2 screenPos = (glm::vec2(lightNDC) * 0.5f + 0.5f) * drawExtent;
    screenPos.y = drawExtent.y - screenPos.y;

    mSunScreenPos = glm::vec4(screenPos, inView ? 1.0f : 0.0f, 0.0f);
}

void Scene3D::editorModeUpdate(EditorCamera& camera) {
    propagateTransform();

    mSceneData.viewPosition = glm::vec4(camera.getPosition(), 0.0f);
    mSceneData.view = camera.getViewMatrix();
    mSceneData.projection = camera.getProjectionMatrix();

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;
    computeSunScreenPos();

    mCameraFov = camera.mFov;
    mNear = camera.mNear;
    mFar = camera.mFar;
}

void Scene3D::update(float deltaTime, const Input& input) {
    // systems
    mScriptManager.update(deltaTime, input);

    mScriptManager.onDestroy(mRegistry);
    cleanupEntities();

    mScriptManager.onInit(mRegistry);
    mScriptManager.onUpdate(mRegistry);
    propagateTransform();

    mScriptManager.onLateUpdate(mRegistry);
    propagateTransform();

    mScriptManager.onCollision(mRegistry);
    propagateTransform();

    mSceneData.view = glm::mat4(1.f);
    mSceneData.projection = glm::mat4(1.f);

    if (mCameraEntity && mCameraEntity->hasComponent<Transform>()) {
        Camera& camera = mCameraEntity->getComponent<Camera>();
        Transform& cameraTransform = mCameraEntity->getComponent<Transform>();

        camera.aspectRatio = mVkEngine.getWindowAspectRatio();
        camera.updateMatrices(cameraTransform);

        mSceneData.viewPosition = glm::vec4(camera.globalPosition, 0.0f);
        mSceneData.view = camera.viewMatrix;
        mSceneData.projection = camera.projectionMatrix;

        mCameraFov = camera.fov;
        mNear = camera.near;
        mFar = camera.far;
    }

    mSceneData.viewProjection = mSceneData.projection * mSceneData.view;
    computeSunScreenPos();
}

void Scene3D::updateUBO(int frameNumber) {
    *(SceneData*)mSceneDataUBOs[frameNumber].buffer.allocInfo.pMappedData = mSceneData;
}

Entity* Scene3D::createEntity(const nlohmann::json& metaJson) {
    return createEntity(componentFromJson<Metadata>(metaJson));
}

Entity* Scene3D::createEntity(Metadata meta) {
    mEntities[meta.uuid] = std::make_unique<Entity>(meta, mRegistry);

    return mEntities[meta.uuid].get();
}

Entity* Scene3D::createEntity(const std::string& name) {
    if (mEntities.contains(name)) {
        fmt::println("Error: An entity named {} already exists", name);
        return nullptr;
    }

    Metadata meta;
    meta.uuid = name;

    return createEntity(meta);
}

Entity* Scene3D::getEntity(const std::string& name) {
    if (!mEntities.contains(name)) {
        return nullptr;
    }

    return mEntities[name].get();
}

void Scene3D::deleteEntity(const std::string& name) {
    mRootEntities.erase(std::remove_if(mRootEntities.begin(), mRootEntities.end(),
        [name](Entity* entity) {
            return entity->getUUID() == name;
        }),
        mRootEntities.end()
    );

    if (mCameraEntity && mCameraEntity->getUUID() == name) {
        mCameraEntity = nullptr;
    }

    mEntities.erase(name);
}

void Scene3D::moveEntity(const std::string& name, const std::string& parent) {
    Entity* entity = getEntity(name);
    Entity* oldParent = entity->getParent();

    // add to root
    if (parent == "") {
        if (oldParent != nullptr) {
            oldParent->removeChild(entity);
        }

        entity->setParent(nullptr);
        if (std::find(mRootEntities.begin(), mRootEntities.end(), entity) == mRootEntities.end()) {
            mRootEntities.push_back(entity);
        }

        return;
    }

    // remove from root
    if (oldParent == nullptr) {
        mRootEntities.erase(std::remove(mRootEntities.begin(), mRootEntities.end(), entity), mRootEntities.end());
    } else {
        oldParent->removeChild(entity);
    }

    Entity* newParent = getEntity(parent);
    newParent->addChild(entity);
    entity->setParent(newParent);
}

void Scene3D::drawConfig(ImGuiContext& context) {
    ImGui::PushFont(context.font12);

    ImGui::PushStyleColor(ImGuiCol_Header, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HOVERED_BUTTON_COLOR);

    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool open = ImGui::CollapsingHeader("Config", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (open) {
        ImGui::PushFont(context.font12);
        ImGui::Indent();

        ImGui::InputFloat3("Light direction", &mSceneData.sunlightDirection.x);
        ImGui::InputFloat3("Light color", &mSceneData.sunlightColor.x);
        ImGui::InputFloat3("Clear color", &mSceneData.clearColor.x);
        ImGui::InputFloat3("Cascade planes", &mSceneData.cascadePlanes.x);
        ImGui::InputFloat2("Shadow bias", &mSceneData.shadowConfig.x);
        ImGui::InputFloat("Shadow N offset", &mSceneData.shadowConfig.z);

        ImGui::Unindent();
        ImGui::PopFont();
    }

    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    open = ImGui::CollapsingHeader("Skybox", ImGuiTreeNodeFlags_DefaultOpen);

    ImGui::PopFont();
    ImGui::PopStyleColor();

    if (open) {
        ImGui::PushFont(context.font12);

        ImGui::Indent();

        ImVec2 size = ImVec2(227, 0); // width, height = 0 for auto
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_FrameBgHovered]);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);

        if (ImGui::Button(mSkybox ? mSkybox->name.c_str() : "None", size)) {
            // optional click behavior
        }

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            mSkybox = nullptr;
            for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                mVkEngine.getPipelineResourceManager().updateSceneDescriptor(
                    mSceneDataUBOs[i].globalDescriptorOffset,
                    mSkybox ? mSkybox->irrMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
                    mSkybox ? mSkybox->prefilteredEnvMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
                    mSkybox ? mSkybox->brdfLut.imageView : mAssetManager.getDefaultImage("brdflut")->imageView,
                    *mAssetManager.getSampler("linear")
                );
            }
        }

        ImGui::PopStyleColor(3);

        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, BUTTON_COLOR);

        // Drag-and-drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SKYBOX_PAYLOAD")) {
                // Handle dropped data here
                std::string newSkybox((char*)payload->Data);
                mSkybox = mAssetManager.getSkybox(newSkybox);
                mSceneData.sunlightDirection = glm::vec4(mSkybox->defaultDirectionalLight, 1.0f);

                for (unsigned int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    mVkEngine.getPipelineResourceManager().updateSceneDescriptor(
                        mSceneDataUBOs[i].globalDescriptorOffset,
                        mSkybox ? mSkybox->irrMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
                        mSkybox ? mSkybox->prefilteredEnvMap.imageView : mAssetManager.getDefaultImage("black_cube")->imageView,
                        mSkybox ? mSkybox->brdfLut.imageView : mAssetManager.getDefaultImage("brdflut")->imageView,
                        *mAssetManager.getSampler("linear")
                    );
                }
            }

            ImGui::EndDragDropTarget();
        }

        ImGui::PopStyleColor();

        ImGui::Unindent();

        ImGui::PopFont();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopFont();
}

void Scene3D::drawInspector(ImGuiContext& context) {
    if (context.selectedEntity == "") return;

    auto& entity = *mEntities[context.selectedEntity];

    context.changedGLTF = false;
    context.changedScript = false;
    context.changedSprite = false;

    ImGui::PushStyleColor(ImGuiCol_Header, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, HOVERED_BUTTON_COLOR);

    UIComponents::forEach([&](auto type) {
        using T = typename decltype(type)::type;

        if (!entity.hasComponent<T>()) {
            return;
        }

        drawComponent<T>(entity.getComponent<T>(), context);
    });

    ImGui::PopStyleColor(3);

    if (context.changedGLTF) {
        auto& component = entity.getComponent<GLTF>();
        component.gltf = mAssetManager.getGltf(component.name);
    }

    if (context.changedSprite) {
        auto& component = entity.getComponent<Sprite>();
        component.image = mAssetManager.getImage(component.name);
    }

    if (context.changedScript) {
        auto& component = entity.getComponent<Script>();
        mVkEngine.getScriptManager().bindScript(component.name, entity);
    }

    if (context.deletedComponent) {
        UIComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;

            if (!entity.hasComponent<T>()) {
                return;
            }

            if (context.componentName != getTypeName<T>()) {
                return;
            }

            if (context.componentName == getTypeName<Camera>()) {
                mCameraEntity = nullptr;
            }

            entity.removeComponent<T>();
        });

        context.componentName = "";
        context.deletedComponent = false;
    }

    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    bool clicked = ImGui::Button("Add component", ImVec2(350 - context.spacing.x * 2, 25));

    if (clicked) {
        ImGui::OpenPopup("AddComponentPopup");
    }

    if (ImGui::BeginPopup("AddComponentPopup")) {
        ImGui::PushStyleColor(ImGuiCol_Text, WHITE);
        UIComponents::forEach([&](auto type) {
            using T = typename decltype(type)::type;

            if (entity.hasComponent<T>()) {
                return;
            }

            // allow only one camera per scene
            if (getTypeName<T>() == getTypeName<Camera>() && mCameraEntity != nullptr) {
                return;
            }

            if (ImGui::MenuItem(getTypeName<T>().c_str())) {
                entity.addComponent<T>(T());

                if (getTypeName<T>() == getTypeName<Camera>()) mCameraEntity = &entity;
            }
        });

        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    ImGui::PopFont();
    ImGui::PopStyleColor(4);
}

void Scene3D::drawHierarchy(ImGuiContext& context) {
    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_DragDropTarget, BUTTON_COLOR);

    for (auto& entity : mRootEntities) {
        entity->drawHierarchy(context);
    }

    ImGui::PushFont(context.normalFont);
    ImGui::PushStyleColor(ImGuiCol_Button, BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HOVERED_BUTTON_COLOR);
    ImGui::PushStyleColor(ImGuiCol_Text, BUTTON_TEXT_COLOR);

    ImVec2 buttonSize = ImVec2(250 - 2 * context.spacing.x, 25);

    static char newNameBuf[100] = {};

    if (mAddingEntity) {
        ImGui::PushStyleColor(ImGuiCol_Text, WHITE);
        ImGui::PushItemWidth(250 - 2 * context.spacing.x);

        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##NewEntityName", newNameBuf, 100, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            std::string newName = std::string(newNameBuf);
            auto newEntity = newName == "" ? nullptr : createEntity(newName);

            if (newEntity != nullptr)
                mRootEntities.push_back(newEntity);

            mAddingEntity = false;
            newNameBuf[0] = 0;
        }

        if (!ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
            mAddingEntity = false;
            newNameBuf[0] = 0;
        }

        ImGui::PopItemWidth();
        ImGui::PopStyleColor();
    }

    if (ImGui::Button("Add entity", buttonSize) && !mAddingEntity) {
        mAddingEntity = true;
    }

    ImGui::Spacing();
    ImGui::InvisibleButton("##DropTarget", ImGui::GetContentRegionAvail());

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_PAYLOAD")) {
            // Handle dropped data here
            std::string movedUUID((char*)payload->Data);
            context.movedEntity = true;
            context.newParent = "";
            context.entityName = movedUUID;
        }
    }

    if (context.deletedEntity) {
        context.selectedEntity = "";
        deleteEntity(context.entityName);

        context.deletedEntity = false;
        context.entityName = "";
    }

    if (context.movedEntity) {
        moveEntity(context.entityName, context.newParent);

        context.movedEntity = false;
        context.entityName = "";
        context.newParent = "";
    }

    ImGui::PopFont();
    ImGui::PopStyleColor(5);

    ImGui::PopFont();
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

    sceneJson["Config"]["lightDirection"] = std::vector<float> {
        mSceneData.sunlightDirection.x,
        mSceneData.sunlightDirection.y,
        mSceneData.sunlightDirection.z
    };

    sceneJson["Config"]["lightColor"] = std::vector<float> {
        mSceneData.sunlightColor.x,
        mSceneData.sunlightColor.y,
        mSceneData.sunlightColor.z
    };

    sceneJson["Config"]["clearColor"] = std::vector<float> {
        mSceneData.clearColor.x,
        mSceneData.clearColor.y,
        mSceneData.clearColor.z
    };

    sceneJson["Config"]["cascadePlanes"] = std::vector<float> {
        mSceneData.cascadePlanes.x,
        mSceneData.cascadePlanes.y,
        mSceneData.cascadePlanes.z
    };

    sceneJson["Config"]["shadowConfig"] = std::vector<float> {
        mSceneData.shadowConfig.x,
        mSceneData.shadowConfig.y,
        mSceneData.shadowConfig.z
    };

    return sceneJson;
}

void Scene3D::saveToFile(const std::filesystem::path& filePath) {
    std::ofstream outFile(filePath);

    outFile << toJson().dump(2);

    outFile.close();
}

void Scene3D::refreshScripts() {
    auto view = mRegistry.view<Script, Metadata>();

    for (auto [entity, script, metadata] : view.each()) {
        mScriptManager.bindScript(script.name, *getEntity(metadata.uuid));
    }
}
