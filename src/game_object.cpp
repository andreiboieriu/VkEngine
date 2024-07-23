#include "game_object.h"
#include "ecs_components/components.h"
#include "ecs_components/transform.h"
#include "fmt/core.h"

GameObject::GameObject() {
    init();
}

GameObject::~GameObject() {
    gRegistry.destroy(mEntity);
}

void GameObject::init() {
    mEntity = gRegistry.create();
}
