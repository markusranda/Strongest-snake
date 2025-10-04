#pragma once
#include "Entity.h"
#include "vector"

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
