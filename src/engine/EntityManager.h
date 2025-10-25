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

struct Material
{
    glm::vec4 color;
    ShaderType shaderType;
    RenderLayer renderLayer;
};

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // --- Dense ---
    std::vector<Transform> transforms;
    std::vector<Quad> quads;
    std::vector<Renderable> renderables;
    std::vector<Material> materials;
    std::vector<Renderable> sortedRenderables;

    // --- Sparse ---
    std::vector<uint32_t> entityToTransform;
    std::vector<uint32_t> entityToQuad;
    std::vector<uint32_t> entityToRenderable;
    std::vector<uint32_t> entityToMaterial;

    std::vector<size_t> transformToEntity;
    std::vector<size_t> quadToEntity;
    std::vector<size_t> renderableToEntity;
    std::vector<size_t> materialToEntity;
    const uint32_t RESIZE_INCREMENT = 2048;

    EntityManager()
    {
        // --- Dense ---
        transforms.reserve(RESIZE_INCREMENT);
        quads.reserve(RESIZE_INCREMENT);
        renderables.reserve(RESIZE_INCREMENT);
    }

    Entity createEntity(Transform transform, Quad quad, Material material)
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

        // ---- Add to stores ----
        Renderable renderable = {entity, makeDrawKey(material.renderLayer, material.shaderType, quad.z, quad.tiebreak)};
        addToStore<Transform>(transforms, entityToTransform, transformToEntity, entity, transform);
        addToStore<Quad>(quads, entityToQuad, quadToEntity, entity, quad);
        addToStore<Renderable>(renderables, entityToRenderable, renderableToEntity, entity, renderable);
        addToStore<Material>(materials, entityToMaterial, materialToEntity, entity, material);
        sortedRenderables.push_back(renderable);

        return entity;
    }

    template <typename A>
    void addToStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &entity, A &item)
    {
        size_t denseIndex = denseStorage.size();
        uint32_t sparseIndex = entityIndex(entity);

        // Do resize
        if (sparseToDense.size() <= sparseIndex)
        {
            sparseToDense.resize(sparseToDense.size() + RESIZE_INCREMENT, UINT32_MAX);
        }
        if (denseToSparse.size() <= denseIndex)
        {
            denseToSparse.resize(denseToSparse.size() + RESIZE_INCREMENT, UINT32_MAX);
        }

        // Add to stores
        denseStorage.push_back(item);
        sparseToDense[sparseIndex] = denseIndex;
        denseToSparse[denseIndex] = sparseIndex;
    }

    void destroyEntity(Entity e)
    {

        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);

        Logger::debug("Destroying entity: " + std::to_string(index));
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

        // --- Remove from stores ---
        removeFromStore<Transform>(transforms, entityToTransform, transformToEntity, e);
        removeFromStore<Quad>(quads, entityToQuad, quadToEntity, e);
        removeFromStore<Renderable>(renderables, entityToRenderable, renderableToEntity, e);

        // --- Sort renderables ---
        {
            ZoneScopedN("Sort renderables");
            sortedRenderables = renderables;
            std::sort(sortedRenderables.begin(), sortedRenderables.end(), [](Renderable &a, Renderable &b)
                      { return a.drawkey < b.drawkey; });
        }
    }

    template <typename A>
    void removeFromStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &e)
    {

        uint32_t denseIndex = sparseToDense[entityIndex(e)];
        uint32_t lastIndex = denseStorage.size() - 1;

        // We ony swap if current denseIndex isn't the last element
        if (denseIndex != lastIndex)
        {
            // Overwrite current slot with last slot
            denseStorage[denseIndex] = std::move(denseStorage[lastIndex]);
            uint32_t movedEntity = denseToSparse[lastIndex];
            denseToSparse[denseIndex] = movedEntity;
            sparseToDense[movedEntity] = denseIndex;
        }

        denseStorage.pop_back();
        denseToSparse.pop_back();
        sparseToDense[entityIndex(e)] = UINT32_MAX;
    }

    bool isAlive(Entity e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }
};
