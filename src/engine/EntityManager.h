#pragma once
#include "vector"
#include <cstdint>
#include <functional>

struct Entity
{
    uint32_t id;

    bool operator==(const Entity &other) const noexcept
    {
        return id == other.id;
    }
};

inline uint32_t entityIndex(Entity e) { return e.id & 0x00FFFFFF; }
inline uint8_t entityGen(Entity e) { return (e.id >> 24) & 0xFF; }

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

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    Entity createEntity()
    {
        uint32_t index;
        if (!freeIndices.empty())
        {
            // Reuse a free slot
            index = freeIndices.back();
            freeIndices.pop_back();
        }
        else
        {
            // Allocate new slot
            index = (uint32_t)generations.size();
            generations.push_back(0);
        }

        uint8_t gen = generations[index];
        return Entity{(gen << 24) | index};
    }

    void destroyEntity(Entity e)
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);

        // Safety: only recycle if it’s actually the right generation
        if (generations[index] == gen)
        {
            generations[index]++;         // bump generation
            freeIndices.push_back(index); // recycle slot
        }
    }

    bool isAlive(Entity e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }

private:
};
