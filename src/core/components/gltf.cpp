#include "gltf.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<GLTF>(const GLTF& component) {
    std::string type = getTypeName<GLTF>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
GLTF componentFromJson(const nlohmann::json& j) {
    GLTF newGLTF{};

    newGLTF.name = j["Name"];

    return newGLTF;
}
