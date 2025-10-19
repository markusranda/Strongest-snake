#pragma once
#include "vector"
#include <cstdint>
#include <functional>
#include <utility>
#include "Transform.h"
#include "Quad.h"
#include "Draworder.h"

struct Entity
{
    uint32_t id;

    bool operator==(const Entity &other) const noexcept
    {
        return id == other.id;
    }
};

inline uint32_t
entityIndex(Entity e)
{
    return e.id & 0x00FFFFFF;
}
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

struct Renderable
{
    Entity entity;
    uint64_t drawkey;
};

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // --- Dense ---
    std::vector<Transform> transforms;
    std::vector<Quad> quads;
    std::vector<Renderable> renderables;

    // --- Sparse ---
    std::vector<uint32_t> entityToTransform;
    std::vector<size_t> transformToEntity;
    std::vector<uint32_t> entityToQuad;
    std::vector<size_t> quadToEntity;
    std::vector<uint32_t> entityToRenderable;
    std::vector<size_t> renderableToEntity;
    const uint32_t RESIZE_INCREMENT = 2048;

    EntityManager()
    {
        // --- Dense ---
        transforms.reserve(RESIZE_INCREMENT);
        quads.reserve(RESIZE_INCREMENT);

        // --- Sparse ---
        entityToTransform.resize(RESIZE_INCREMENT, UINT32_MAX);
        entityToQuad.resize(RESIZE_INCREMENT, UINT32_MAX);
        entityToRenderable.resize(RESIZE_INCREMENT, UINT32_MAX);
    }

    Entity createEntity(Transform transform, Quad quad)
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
        Entity entity = Entity{(gen << 24) | index};

        // Transforms
        transforms.push_back(transform);
        if (entityToTransform.size() <= entityIndex(entity))
        {
            entityToTransform.resize(entityIndex(entity) + RESIZE_INCREMENT, UINT32_MAX);
        }
        entityToTransform[entityIndex(entity)] = transforms.size() - 1;
        if (transformToEntity.size() <= entityIndex(entity))
        {
            transformToEntity.resize(entityIndex(entity) + RESIZE_INCREMENT, UINT32_MAX);
        }
        transformToEntity[transforms.size() - 1] = entityIndex(entity);

        // Quads
        quads.push_back(quad);
        if (entityToQuad.size() <= entityIndex(entity))
        {
            entityToQuad.resize(entityToQuad.size() + RESIZE_INCREMENT, UINT32_MAX);
        }
        entityToQuad[entityIndex(entity)] = quads.size() - 1;
        if (quadToEntity.size() <= entityIndex(entity))
        {
            quadToEntity.resize(quadToEntity.size() + RESIZE_INCREMENT, UINT32_MAX);
        }
        quadToEntity[quads.size() - 1] = entityIndex(entity);

        // Renderables
        Renderable renderable = {entity, makeDrawKey(quad.renderLayer, quad.shaderType, quad.z, quad.tiebreak)};
        renderables.push_back(renderable);
        if (entityToRenderable.size() <= entityIndex(entity))
        {
            entityToRenderable.resize(entityToRenderable.size() + RESIZE_INCREMENT, UINT32_MAX);
        }
        entityToRenderable[entityIndex(entity)] = renderables.size() - 1;

        if (renderableToEntity.size() <= entityIndex(entity))
        {
            renderableToEntity.resize(renderableToEntity.size() + RESIZE_INCREMENT, UINT32_MAX);
        }
        renderableToEntity[renderables.size() - 1] = entityIndex(entity);

        return entity;
    }

    void destroyEntity(Entity e)
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);

        if (index >= generations.size())
            return;

        // Safety: only recycle if it’s actually the right generation
        if (generations[index] == gen)
        {
            if (++generations[index] == 0)
                generations[index] = 1;
            else
                generations[index]++;     // bump generation
            freeIndices.push_back(index); // recycle slot
        }

        // --- Transforms ---
        {
            uint32_t tIndex = entityToTransform[entityIndex(e)];
            uint32_t tLastIndex = transforms.size() - 1;

            // We ony swap if current index isn't the last element
            if (tIndex != tLastIndex)
            {
                transforms[tIndex] = std::move(transforms[tLastIndex]);

                uint32_t movedEntity = transformToEntity[tLastIndex];
                if (movedEntity != UINT32_MAX)
                {
                    transformToEntity[tIndex] = movedEntity;
                    entityToTransform[movedEntity] = tIndex;
                }
            }

            transforms.pop_back();
            transformToEntity.pop_back();
            entityToTransform[entityIndex(e)] = UINT32_MAX;
            ;
        }

        // --- Quads ---
        {
            uint32_t qIndex = entityToQuad[entityIndex(e)];
            uint32_t qLastIndex = quads.size() - 1;

            // We ony swap if current index isn't the last element
            if (qIndex != qLastIndex)
            {
                quads[qIndex] = std::move(quads[qLastIndex]);

                uint32_t movedEntity = quadToEntity[qLastIndex];
                if (movedEntity != UINT32_MAX)
                {
                    quadToEntity[qIndex] = movedEntity;
                    entityToQuad[movedEntity] = qIndex;
                }
            }

            quads.pop_back();
            quadToEntity.pop_back();
            entityToQuad[entityIndex(e)] = UINT32_MAX;
        }

        // --- Renderable ---
        {
            uint32_t index = entityToRenderable[entityIndex(e)];
            uint32_t lastIndex = renderables.size() - 1;

            // We ony swap if current index isn't the last element
            if (index != lastIndex)
            {
                renderables[index] = std::move(renderables[lastIndex]);

                uint32_t movedEntity = renderableToEntity[lastIndex];
                if (movedEntity != UINT32_MAX)
                {
                    renderableToEntity[index] = movedEntity;
                    entityToRenderable[movedEntity] = index;
                }
            }

            renderables.pop_back();
            renderableToEntity.pop_back();
            entityToRenderable[entityIndex(e)] = UINT32_MAX;
        }
    }

    bool isAlive(Entity e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }
};
