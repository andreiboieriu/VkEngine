#pragma once

#include "vk_loader.h"
#include "vk_materials.h"
#include <memory>

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct RenderContext {
    std::vector<RenderObject> opaqueObjects;
};

class IRenderable {
public:

    virtual void draw(const glm::mat4& topMatrix, RenderContext& context) = 0;
};

class Node : public IRenderable {
public:

    void refreshTransform(const glm::mat4& parentMatrix);
    virtual void draw(const glm::mat4& topMatrix, RenderContext& context);

    void setLocalTransform(glm::mat4 transform) {
        mLocalTransform = transform;
    }

    void setWorldTransform(glm::mat4 transform) {
        mWorldTransform = transform;
    }

protected:

    std::weak_ptr<Node> mParent;
    std::vector<std::shared_ptr<Node>> mChildren;

    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;
};

class MeshNode : public Node {
public:

    virtual void draw(const glm::mat4& topMatrix, RenderContext& context) override;

    void setMesh(std::shared_ptr<MeshAsset> mesh) {
        mMesh = mesh;
    }

    std::shared_ptr<MeshAsset> getMesh() {
        return mMesh;
    }

private:

    std::shared_ptr<MeshAsset> mMesh;
};
