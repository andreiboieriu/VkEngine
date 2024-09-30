#pragma once

#include <string>

class UUID {

public:
    UUID();
    UUID(const std::string& name);
    
    static const UUID null();

    std::string toString() const {
        return mUUID;
    }

    operator std::string() const {
        return mUUID;
    }

    bool operator==(const UUID& u) const {
        return mUUID == u.mUUID;
    }

    bool operator!=(const UUID& u) const {
        return mUUID != u.mUUID;
    }

private:

    inline static int nextID = 0;
    inline static const std::string defaultBaseID = "Entity";
    inline static const std::string nullID = "";
    std::string mUUID;
};

namespace std {
    template<typename T>
    struct hash;

    template<>
    struct hash<UUID> {
        std::size_t operator()(const UUID& u) const {
            return std::hash<std::string>{}(u);
        }
    };
};
