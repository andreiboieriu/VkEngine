#pragma once

#include "fmt/core.h"
#include "vk_descriptors.h"
#include "vk_materials.h"
#include <memory>
#include <string>
#include <string_view>
#include <vk_types.h>
#include <unordered_map>

class GLTFNode {
public:

    GLTFNode(std::shared_ptr<MeshAsset> mesh = nullptr);

    void draw(const glm::mat4& transform, RenderContext& context);

    void addChild(std::shared_ptr<GLTFNode> child);

    bool hasParent();
    void setParent(std::weak_ptr<GLTFNode> parent);

    void propagateTransform(const glm::mat4& transform = glm::mat4(1.f));

    void setLocalTransform(glm::mat4 transform);
    void setGlobalTransform(glm::mat4 transform);

    glm::mat4& getLocalTransform();
    glm::mat4& getGlobalTransform();

private:

    std::weak_ptr<GLTFNode> mParent;
    std::vector<std::shared_ptr<GLTFNode>> mChildren;
    std::shared_ptr<MeshAsset> mMesh;

    glm::mat4 mLocalTransform;
    glm::mat4 mGlobalTransform;
};

class LoadedGLTF {
public:
    LoadedGLTF(std::string_view filePath, DescriptorManager& descriptorManager);
    ~LoadedGLTF();

    void draw(const glm::mat4& transform, RenderContext& context);
    void freeResources();

private:
    void load(std::string_view filePath, DescriptorManager& descriptorManager);

    void initMaterialDataBuffer(size_t materialCount);

    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> mMeshes;
    std::unordered_map<std::string, std::shared_ptr<GLTFNode>> mNodes;
    std::unordered_map<std::string, AllocatedImage> mImages;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> mMaterials;

    std::vector<std::shared_ptr<GLTFNode>> mTopNodes;

    std::vector<VkSampler> mSamplers;
    AllocatedBuffer mMaterialDataBuffer;
};