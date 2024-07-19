#include "vk_scene.h"
#include "vk_types.h"

IRenderable::~IRenderable() {}

void Node::refreshTransform(const glm::mat4& parentMatrix) {
    mWorldTransform = parentMatrix * mLocalTransform;

    for (auto& child : mChildren) {
        child->refreshTransform(mWorldTransform);
    }
}

void Node::draw(const glm::mat4& topMatrix, RenderContext& context) {
    for (auto& child : mChildren) {
        child->draw(topMatrix, context);
    }
}

void MeshNode::draw(const glm::mat4& topMatrix, RenderContext& context) {
    glm::mat4 nodeMatrix = topMatrix * mWorldTransform;

    for (auto& surface : mMesh->surfaces) {
        RenderObject object;

        object.indexCount = surface.count;
        object.firstIndex = surface.startIndex;
        object.indexBuffer = mMesh->meshBuffers.indexBuffer.buffer;
        object.material = &surface.material->materialInstance;
        object.bounds = surface.bounds;
        object.transform = nodeMatrix;
        object.vertexBufferAddress = mMesh->meshBuffers.vertexBufferAddress;

        if (surface.material->materialInstance.passType == MaterialPass::Opaque) {
            context.opaqueObjects.push_back(object);
        } else {
            context.transparentObjects.push_back(object);
        }
    }

    Node::draw(topMatrix, context);
}
