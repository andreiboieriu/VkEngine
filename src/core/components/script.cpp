#include "script.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<Script>(const Script& component) {
    std::string type = getTypeName<Script>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
Script componentFromJson(const nlohmann::json& j) {
    Script newScript{};

    newScript.name = j["Name"];

    return newScript;
}
