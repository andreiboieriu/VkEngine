#pragma once

#include <sol/sol.hpp>

struct Script {
    std::string name = "";
    sol::environment env;
    std::unordered_map<std::string, sol::type> symbols;
    bool initialized = false;
};
