#pragma once
#include "vector"
#include "components/Transform.h"
#include "components/AABB.h"
#include "components/Mesh.h"
#include "components/Entity.h"
#include "components/Health.h"
#include "components/Treasure.h"
#include "components/EntityType.h"
#include "components/Renderable.h"
#include "components/Material.h"
#include "Collision.h"
#include "DrawCmd.h"
#include "TextureComponent.h"
#include "../libs/ankerl/unordered_dense.h"
#include "Chunk.h"
#include "SnakeMath.h"
#include <cstdint>
#include <functional>
#include <stack>
#include <queue>
#include <utility>

constexpr uint32_t MAX_SEARCH_ITERATIONS_ATTEMPTS = UINT32_MAX / 2;

enum class SpatialStorage : uint32_t
{
    Global,
    Chunk,
    ChunkTile,
};

struct EntityManager
{
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // --- Dense ---
    std::vector<Transform> transforms;
    std::vector<Mesh> meshes;
    std::vector<Renderable> renderables;
    std::vector<Material> materials;
    std::vector<AABB> collisionBoxes;
    std::vector<glm::vec4> uvTransforms;
    std::vector<EntityType> entityTypes;
    std::vector<Entity> activeEntities;
    std::vector<Health> healths;
    std::vector<Treasure> treasures;

    // --- Sparse ---
    std::vector<uint32_t> entityToTransform;
    std::vector<uint32_t> entityToMesh;
    std::vector<uint32_t> entityToRenderable;
    std::vector<uint32_t> entityToMaterial;
    std::vector<uint32_t> entityToCollisionBox;
    std::vector<uint32_t> entityToUvTransforms;
    std::vector<uint32_t> entityToEntityTypes;
    std::vector<uint32_t> entityToHealth;
    std::vector<uint32_t> entityToTreasure;

    std::vector<size_t> transformToEntity;
    std::vector<size_t> meshToEntity;
    std::vector<size_t> renderableToEntity;
    std::vector<size_t> materialToEntity;
    std::vector<size_t> collisionBoxToEntity;
    std::vector<size_t> uvTransformsToEntity;
    std::vector<size_t> entityTypesToEntity;
    std::vector<size_t> healthToEntity;
    std::vector<size_t> treasureToEntity;

    // Spatial storage of entities
    ankerl::unordered_dense::map<int64_t, Chunk> chunks;
    std::vector<Entity> globalEntities;

    const uint32_t RESIZE_INCREMENT = 2048;

    EntityManager()
    {
        // --- Dense ---
        transforms.reserve(RESIZE_INCREMENT);
        meshes.reserve(RESIZE_INCREMENT);
        renderables.reserve(RESIZE_INCREMENT);
        freeIndices.reserve(RESIZE_INCREMENT);
    }

    Entity createEntity(
        Transform transform,
        Mesh mesh,
        Material material,
        RenderLayer renderLayer,
        EntityType entityType,
        const SpatialStorage &spatialStorage,
        glm::vec4 uvTransform = glm::vec4{},
        float z = 0.0f)
    {
        // ---- Generate entity ----
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

        // --- Add to spatial storage ---
        switch (spatialStorage)
        {
        case SpatialStorage::Global:
        {
            globalEntities.push_back(entity);
            break;
        }
        case SpatialStorage::Chunk:
        {
            insertEntityInChunk(entity, transform);
            break;
        }
        case SpatialStorage::ChunkTile:
        {
            insertEntityInChunkTile(entity, transform);
            break;
        }
        }

        // ---- Add to stores ----
        Renderable renderable = {entity, z, 0, renderLayer};
        renderable.makeDrawKey(material.shaderType, material.atlasIndex, mesh.vertexOffset, mesh.vertexCount);
        AABB aabb = computeWorldAABB(mesh, transform);

        addToStore<Transform>(transforms, entityToTransform, transformToEntity, entity, transform);
        addToStore<Mesh>(meshes, entityToMesh, meshToEntity, entity, mesh);
        addToStore<Renderable>(renderables, entityToRenderable, renderableToEntity, entity, renderable);
        addToStore<Material>(materials, entityToMaterial, materialToEntity, entity, material);
        addToStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, entity, uvTransform);
        addToStore<EntityType>(entityTypes, entityToEntityTypes, entityTypesToEntity, entity, entityType);
        addToStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, entity, aabb);

        return entity;
    }

    void destroyEntity(Entity e, const SpatialStorage &spatialStorage)
    {
        ZoneScoped;

        uint32_t entityIdx = entityIndex(e);
        uint8_t gen = entityGen(e);

        if (entityIdx >= generations.size())
            return;

        // Safety: only recycle if it’s actually the right generation
        if (generations[entityIdx] == gen)
        {
            if (++generations[entityIdx] == 0) // Handle wrapping
                generations[entityIdx] = 1;
            else
                generations[entityIdx]++;     // bump generation
            freeIndices.push_back(entityIdx); // recycle slot
        }

        // --- Remove from spatial storage ---
        switch (spatialStorage)
        {
        case SpatialStorage::Global:
        {
            deleteEntityFromGlobal(entityIdx);
            break;
        }
        case SpatialStorage::Chunk:
        {
            AABB &aabb = collisionBoxes[entityToCollisionBox[entityIdx]];
            deleteEntityFromChunk(entityIdx, aabb);
            break;
        }
        case SpatialStorage::ChunkTile:
        {
            AABB &aabb = collisionBoxes[entityToCollisionBox[entityIdx]];
            deleteEntityFromChunkTile(aabb);
            break;
        }
        }

        // --- Remove from stores ---
        // TODO One day you need to figure out a more performant way to clean shit up
        {
            ZoneScopedN("Remove from stores");

            removeFromStore<Transform>(transforms, entityToTransform, transformToEntity, e);
            removeFromStore<Mesh>(meshes, entityToMesh, meshToEntity, e);
            removeFromStore<Renderable>(renderables, entityToRenderable, renderableToEntity, e);
            removeFromStore<Material>(materials, entityToMaterial, materialToEntity, e);
            removeFromStore<glm::vec4>(uvTransforms, entityToUvTransforms, uvTransformsToEntity, e);
            removeFromStore<EntityType>(entityTypes, entityToEntityTypes, entityTypesToEntity, e);
            if (entityIdx < entityToHealth.size() && entityToHealth[entityIdx] != UINT32_MAX)
                removeFromStore<Health>(healths, entityToHealth, healthToEntity, e);
            if (entityIdx < entityToTreasure.size() && entityToTreasure[entityIdx] != UINT32_MAX)
                removeFromStore<Treasure>(treasures, entityToTreasure, treasureToEntity, e);
            removeFromStore<AABB>(collisionBoxes, entityToCollisionBox, collisionBoxToEntity, e);
        }
    }

    template <typename A>
    void addToStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &entity, A &item)
    {
        ZoneScoped;

        size_t denseIndex = denseStorage.size();
        uint32_t sparseIndex = entityIndex(entity);

        // Do resize
        if (sparseToDense.size() <= sparseIndex)
        {
            size_t newSize = ((sparseIndex / RESIZE_INCREMENT) + 1) * RESIZE_INCREMENT;
            sparseToDense.resize(newSize, UINT32_MAX);
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

    template <typename A>
    void removeFromStore(std::vector<A> &denseStorage, std::vector<uint32_t> &sparseToDense, std::vector<size_t> &denseToSparse, Entity &e)
    {
        ZoneScoped;

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

    bool isAlive(Entity &e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }

    void getAllRelevantEntities(const Transform &player, std::vector<Entity> &entities)
    {
        entities.insert(entities.end(), globalEntities.begin(), globalEntities.end());
        getIntersectingEntitiesChunks(player, entities);
    }

    void getIntersectingEntitiesChunks(const Transform &player, std::vector<Entity> &entities)
    {
        ZoneScoped;

        Entity localBuf[9 * TILES_PER_CHUNK];
        size_t count = 0;
        int32_t cx = worldPosToClosestChunk(player.position.x);
        int32_t cy = worldPosToClosestChunk(player.position.y);

        for (int dx = -1; dx <= 1; dx++)
        {
            for (int dy = -1; dy <= 1; dy++)
            {
                int32_t chunkWorldX = cx + dx * CHUNK_WORLD_SIZE;
                int32_t chunkWorldY = cy + dy * CHUNK_WORLD_SIZE;
                int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);
                auto it = chunks.find(chunkIdx);
                assert(it != chunks.end());
                entities.insert(entities.end(), it->second.staticEntities.begin(), it->second.staticEntities.end());

                for (size_t i = 0; i < TILES_PER_CHUNK; i += 8)
                {
                    const Entity &e0 = it->second.tiles[i + 0];
                    const Entity &e1 = it->second.tiles[i + 1];
                    const Entity &e2 = it->second.tiles[i + 2];
                    const Entity &e3 = it->second.tiles[i + 3];
                    const Entity &e4 = it->second.tiles[i + 4];
                    const Entity &e5 = it->second.tiles[i + 5];
                    const Entity &e6 = it->second.tiles[i + 6];
                    const Entity &e7 = it->second.tiles[i + 7];

                    if (e0.id != UINT32_MAX)
                        localBuf[count++] = e0;
                    if (e1.id != UINT32_MAX)
                        localBuf[count++] = e1;
                    if (e2.id != UINT32_MAX)
                        localBuf[count++] = e2;
                    if (e3.id != UINT32_MAX)
                        localBuf[count++] = e3;
                    if (e4.id != UINT32_MAX)
                        localBuf[count++] = e4;
                    if (e5.id != UINT32_MAX)
                        localBuf[count++] = e5;
                    if (e6.id != UINT32_MAX)
                        localBuf[count++] = e6;
                    if (e7.id != UINT32_MAX)
                        localBuf[count++] = e7;
                }
            }
        }
        entities.insert(entities.end(), localBuf, localBuf + count);
    }

    void deleteEntityFromGlobal(uint32_t &entityIdx)
    {
        size_t foundAt = globalEntities.size();
        for (size_t i = 0; i < globalEntities.size(); i++)
        {
            if (entityIndex(globalEntities[i]) == entityIdx)
            {
                foundAt = i;
                break;
            }
        }
        assert(foundAt != globalEntities.size() && "Entity not found in globalEntities");
        globalEntities[foundAt] = globalEntities.back();
        globalEntities.pop_back();
    }

    void deleteEntityFromChunk(uint32_t &entityIdx, const AABB &aabb)
    {
        ZoneScoped;

        int32_t chunkWorldX = worldPosToClosestChunk(aabb.min.x);
        int32_t chunkWorldY = worldPosToClosestChunk(aabb.min.y);
        int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);
        auto it = chunks.find(chunkIdx);
        assert(it != chunks.end());
        Chunk &chunk = it->second;

        size_t foundAt = chunk.staticEntities.size();
        for (size_t i = 0; i < chunk.staticEntities.size(); i++)
        {
            if (entityIndex(chunk.staticEntities[i]) == entityIdx)
            {
                foundAt = i;
                break;
            }
        }
        assert(foundAt != chunk.staticEntities.size() && "Entity not found in chunk.staticEntities");
        chunk.staticEntities[foundAt] = chunk.staticEntities.back();
        chunk.staticEntities.pop_back();
    }

    void deleteEntityFromChunkTile(const AABB &aabb)
    {
        ZoneScoped;

        int32_t chunkWorldX = worldPosToClosestChunk(aabb.min.x);
        int32_t chunkWorldY = worldPosToClosestChunk(aabb.min.y);
        int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);
        auto it = chunks.find(chunkIdx);
        assert(it != chunks.end());
        Chunk &chunk = it->second;

        int32_t localTileX = ((int32_t)aabb.min.x - chunkWorldX) / TILE_WORLD_SIZE;
        int32_t localTileY = ((int32_t)aabb.min.y - chunkWorldY) / TILE_WORLD_SIZE;
        size_t tileIdx = localIndexToTileIndex(localTileX, localTileY);
        Entity &entity = chunk.tiles[tileIdx];
        entity.id = UINT32_MAX;
    }

    void collectChunkDebugInstances(std::vector<InstanceData> &instances)
    {
        for (auto &[key, val] : chunks)
        {
            Transform t = Transform(glm::vec2{val.chunkX, val.chunkY}, glm::vec2{CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE});
            glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 0.25f); // translucent red
            InstanceData instanceData(t.model, color, glm::vec4(0, 0, 1, 1), glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f});
            instances.push_back(instanceData);
        }
    }

    inline void insertEntityInChunk(Entity entity, Transform &transform)
    {
        int32_t tileWorldX = transform.position.x;
        int32_t tileWorldY = transform.position.y;
        int32_t chunkWorldX = worldPosToClosestChunk(tileWorldX);
        int32_t chunkWorldY = worldPosToClosestChunk(tileWorldY);
        int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);
        auto it = chunks.find(chunkIdx);
        assert(it != chunks.end());
        Chunk &chunk = it->second;

        chunk.staticEntities.push_back(entity);
    }

    inline void insertEntityInChunkTile(Entity entity, Transform &transform)
    {
        int32_t tileWorldX = transform.position.x;
        int32_t tileWorldY = transform.position.y;
        int32_t chunkWorldX = worldPosToClosestChunk(tileWorldX);
        int32_t chunkWorldY = worldPosToClosestChunk(tileWorldY);
        int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);
        auto it = chunks.find(chunkIdx);
        assert(it != chunks.end());
        Chunk &chunk = it->second;

        int32_t localTileX_world = tileWorldX - chunkWorldX;
        int32_t localTileY_world = tileWorldY - chunkWorldY;
        int32_t localTileX = localTileX_world / TILE_WORLD_SIZE;
        int32_t localTileY = localTileY_world / TILE_WORLD_SIZE;

        assert(localTileX >= 0 && localTileX < TILES_PER_ROW);
        assert(localTileY >= 0 && localTileY < TILES_PER_ROW);
        int32_t tileIdx = localIndexToTileIndex(localTileX, localTileY);
        assert(tileIdx >= 0 && tileIdx < TILES_PER_CHUNK);

        chunk.tiles[tileIdx] = entity;
    }
};
