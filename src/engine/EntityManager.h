#pragma once
#include "vector"
#include "Transform.h"
#include "AABB.h"
#include "Collision.h"
#include "Draworder.h"
#include "Mesh.h"
#include "TextureComponent.h"
#include <cstdint>
#include <functional>
#include <utility>

struct Entity
{
    uint32_t id;

    bool operator==(const Entity &other) const noexcept
    {
        return id == other.id;
    }
};

inline uint32_t entityIndex(Entity e)
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
    float z = 0.0f;
    uint8_t tiebreak = 0;
    RenderLayer renderLayer;
    uint64_t drawkey;

    // 64-bit draw key
    // [ 8 bits layer ] [ 16 bits shaderType ] [ 32 bits z-sort value ] [ 8 bits tie-break ]
    // | 63 .... 56 | 55 .... 40 | 39 ............ 8 | 7 .... 0 |
    // |   Layer    |  Shader    |     Z-Sort       |   Tie    |
    inline void makeDrawKey(ShaderType shader)
    {
        static_assert(sizeof(RenderLayer) == 1, "RenderLayer must be 8 bits");
        static_assert(sizeof(ShaderType) == 2, "ShaderType must be 16 bits");

        uint64_t key = 0;
        key |= (uint64_t(renderLayer) & 0xFF) << 56;
        key |= (uint64_t(shader) & 0xFFFF) << 40;
        uint32_t zBits = static_cast<uint32_t>((1.0f - z) * 0xFFFFFFFFu);
        key |= (uint64_t)zBits << 8;
        key |= tiebreak;

        // Commit
        drawkey = key;
    }
};

struct Material
{
    glm::vec4 color;
    ShaderType shaderType;
};

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // --- Dense ---
    std::vector<Transform> transforms;
    std::vector<Mesh> meshes;
    std::vector<Renderable> renderables;
    std::vector<Renderable> sortedRenderables;
    std::vector<Material> materials;
    std::vector<AABB> collisionBoxes;
    std::vector<glm::vec4> uvTransforms;

    // --- Sparse ---
    std::vector<uint32_t> entityToTransform;
    std::vector<uint32_t> entityToMesh;
    std::vector<uint32_t> entityToRenderable;
    std::vector<uint32_t> entityToMaterial;
    std::vector<uint32_t> entityToCollisionBox;
    std::vector<uint32_t> entityToUvTransforms;

    std::vector<size_t> transformToEntity;
    std::vector<size_t> meshToEntity;
    std::vector<size_t> renderableToEntity;
    std::vector<size_t> materialToEntity;
    std::vector<size_t> collisionBoxToEntity;
    std::vector<size_t> uvTransformsToEntity;

    const uint32_t RESIZE_INCREMENT = 2048;

    EntityManager()
    {
        // --- Dense ---
        transforms.reserve(RESIZE_INCREMENT);
        meshes.reserve(RESIZE_INCREMENT);
        renderables.reserve(RESIZE_INCREMENT);
    }

    Entity createEntity(
        Transform transform,
        Mesh mesh,
        Material material,
        RenderLayer renderLayer,
        glm::vec4 uvTransform = glm::vec4{})
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
        Renderable renderable = {entity, 0.0f, 0, renderLayer};
        renderable.makeDrawKey(material.shaderType);
        AABB aabb = computeWorldAABB(mesh, transform);
        addToStore<Transform>(transforms, entityToTransform, transformToEntity, entity, transform);
        addToStore<Mesh>(meshes, entityToMesh, meshToEntity, entity, mesh);
        addToStore<Renderable>(renderables, entityToRenderable, renderableToEntity, entity, renderable);
        addToStore<Material>(materials, entityToMaterial, materialToEntity, entity, material);
        addToStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, entity, aabb);
        addToStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, entity, uvTransform);

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
        removeFromStore<Mesh>(meshes, entityToMesh, meshToEntity, e);
        removeFromStore<Renderable>(renderables, entityToRenderable, renderableToEntity, e);
        removeFromStore<Material>(materials, entityToMaterial, materialToEntity, e);
        if (entityToCollisionBox[entityIndex(e)])
            removeFromStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, e);
        removeFromStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, e);

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
