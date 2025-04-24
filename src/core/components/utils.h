#pragma once

#include <nlohmann/json.hpp>
#include <cxxabi.h>

template<typename T>
nlohmann::json componentToJson(const T& component);

template<typename T>
T componentFromJson(const nlohmann::json& j);

// returns demangled type name
template <typename T>
std::string getTypeName() {
    int status;
    std::unique_ptr<char[], void(*)(void*)> res {
        abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status),
        std::free
    };

    return (status == 0) ? res.get() : typeid(T).name();
}

// type list for component types
template<typename... Component>
struct ComponentList{
    template<typename F>
    static void forEach(F&& f) {
        (f(std::type_identity<Component>{}), ...);
    }
};

// forward references, so each component can have its separate header
struct GLTF;
struct Transform;
struct Script;
struct Destroy;
struct SphereCollider;
struct Camera;
struct Metadata;
struct Sprite;

using AllComponents = ComponentList<GLTF, Transform, Script, Destroy, SphereCollider, Camera, Metadata, Sprite>;
using SerializableComponents = ComponentList<GLTF, Transform, Script, SphereCollider, Camera, Metadata, Sprite>;
