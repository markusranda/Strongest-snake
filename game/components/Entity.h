#pragma once
#include <cstdint>
#include <utility>

static const uint32_t ENTITY_SENTINEL_ID = 0xFFFFFFFF;

struct Entity
{
    uint32_t id;

    Entity() : id(ENTITY_SENTINEL_ID) {}
    Entity(uint32_t id) : id(id) {}

    bool operator==(const Entity &other) const noexcept
    {
        return id == other.id;
    }

    bool operator!=(const Entity &other) const noexcept
    {
        return id != other.id;
    }
};

inline uint32_t entityIndex(Entity &e)
{
    return e.id & 0x00FFFFFF;
}

inline uint8_t entityGen(Entity &e) { return (e.id >> 24) & 0xFF; }


inline bool entityUnset(Entity &e) { return e.id == ENTITY_SENTINEL_ID; };

// Specialize std::hash for Entity
namespace std
{
    template <>
    struct hash<Entity>
    {
        std::size_t operator()(const Entity &e) const noexcept
        {
            return std::hash<uint32_t>{}(e.id);
        }
    };
}