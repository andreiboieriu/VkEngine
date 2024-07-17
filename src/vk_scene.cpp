#include "vk_scene.h"

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

        object.transform = nodeMatrix;
        object.vertexBufferAddress = mMesh->meshBuffers.vertexBufferAddress;

        context.opaqueObjects.push_back(object);
    }

    Node::draw(topMatrix, context);
}
