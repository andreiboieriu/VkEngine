#include "metadata.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<Metadata>(const Metadata& component) {
    std::string type = getTypeName<Metadata>();
    nlohmann::json j;

    j[type]["UUID"] = component.uuid;

    return j;
}

template<>
Metadata componentFromJson(const nlohmann::json& j) {
    Metadata newMetadata{};

    newMetadata.uuid = j["UUID"];

    return newMetadata;
}
