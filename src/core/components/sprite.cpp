#include "sprite.h"
#include "utils.h"

#include <nlohmann/json.hpp>

template<>
nlohmann::json componentToJson<Sprite>(const Sprite& component) {
    std::string type = getTypeName<Sprite>();
    nlohmann::json j;

    j[type]["Name"] = component.name;

    return j;
}

template<>
Sprite componentFromJson(const nlohmann::json& j) {
    Sprite newSprite{};

    newSprite.name = j["Name"];

    return newSprite;
}
