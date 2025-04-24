#include "sphere_collider.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<SphereCollider>(const SphereCollider& component) {
    std::string type = getTypeName<SphereCollider>();
    nlohmann::json j;

    j[type]["Radius"] = component.radius;

    return j;
}

template<>
SphereCollider componentFromJson(const nlohmann::json& j) {
    SphereCollider newSphereCollider{};

    newSphereCollider.radius = j["Radius"];

    return newSphereCollider;
}
