#include "scene_node.h"
#include "ecs_components/components.h"
#include "ecs_components/transform.h"
#include "fmt/core.h"

SceneNode::SceneNode(std::string_view name) : mName(name) {
    init();
}

SceneNode::~SceneNode() {
    gRegistry.destroy(mEntity);
}

void SceneNode::init() {
    mEntity = gRegistry.create();
}
