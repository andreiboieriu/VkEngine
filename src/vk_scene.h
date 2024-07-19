#pragma once

#include "vk_types.h"
#include <memory>

class IRenderable {
public:
    virtual ~IRenderable() = 0;
    virtual void draw(const glm::mat4& topMatrix, RenderContext& context) = 0;
};

class Node : public IRenderable {
public:

    void refreshTransform(const glm::mat4& parentMatrix);
    void draw(const glm::mat4& topMatrix, RenderContext& context);

    void setLocalTransform(glm::mat4 transform) {
        mLocalTransform = transform;
    }

    void setWorldTransform(glm::mat4 transform) {
        mWorldTransform = transform;
    }

    glm::mat4& getLocalTransform() {
        return mLocalTransform;
    }

    void addChild(std::shared_ptr<Node> child) {
        mChildren.push_back(child);
    }

    void setParent(std::weak_ptr<Node> parent) {
        mParent = parent;
    }

    bool hasParent() {
        return mParent.lock() != nullptr;
    }

    std::vector<std::shared_ptr<Node>>& getChildren() {
        return mChildren;
    }

protected:

    std::weak_ptr<Node> mParent;
    std::vector<std::shared_ptr<Node>> mChildren;

    glm::mat4 mLocalTransform;
    glm::mat4 mWorldTransform;
};

class MeshNode : public Node {
public:

    void draw(const glm::mat4& topMatrix, RenderContext& context) override;

    void setMesh(std::shared_ptr<MeshAsset> mesh) {
        mMesh = mesh;
    }

    std::shared_ptr<MeshAsset> getMesh() {
        return mMesh;
    }

private:

    std::shared_ptr<MeshAsset> mMesh;
};
