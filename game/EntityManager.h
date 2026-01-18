#pragma once
#include "vector"
#include "components/Transform.h"
#include "components/AABB.h"
#include "components/Mesh.h"
#include "components/Entity.h"
#include "components/Health.h"
#include "components/Ground.h"
#include "components/GroundCosmetic.h"
#include "components/EntityType.h"
#include "components/Renderable.h"
#include "components/Material.h"
#include "components/GroundOre.h"
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

enum class ComponentId : uint16_t
{
    AABB,
    Transform,
    Mesh,
    Renderable,
    Material,
    UvTransform,
    EntityType,
    Health,
    Ground,
    GroundCosmetic,
    GroundOre,
    COUNT,
};

struct ComponentStorage {
    // --- Dense ---
    uint8_t *denseBytes = nullptr;
    uint32_t denseSize = 0;
    uint32_t denseCapacity = 0;

    // --- Entity -> Dense ---
    uint32_t *entityToDense = nullptr;
    uint32_t  entityToDenseCapacity = 0;

    // --- Dense -> Entity ---
    uint32_t *denseToEntity = nullptr;
    
    // --- Component ---
    ComponentId componentId;
    uint32_t componentSize;

    static constexpr uint32_t SENTINEL = UINT32_MAX;
    static constexpr uint32_t MEM_CHUNK_SIZE = 0x10000;

    ComponentStorage(ComponentId componentId, uint32_t componentSize): componentId(componentId), componentSize(componentSize) {
        growDense();
        growEntityToDense(1024);
    }

    uint8_t *push(const void *src, uint32_t entityIdx)
    {
        // --- Dense ---
        if (denseSize == denseCapacity) {
            growDense();
        }
        uint32_t denseIdx = denseSize;
        void *dst = denseBytes + size_t(denseIdx * componentSize);
        memcpy(dst, src, componentSize);
        denseToEntity[denseIdx] = entityIdx;
        
        // --- Entity -> Dense ---
        if (entityIdx >= entityToDenseCapacity) {
            growEntityToDense(entityIdx + 1);
        }
        entityToDense[entityIdx] = denseIdx;

        denseSize++;
        assert(denseSize <= denseCapacity);
        assert(entityIdx < entityToDenseCapacity);

        return (uint8_t *)dst;
    }

    // Returns nullptr if thing doesn't exist
    uint8_t *find(uint32_t entityIdx) {
        if (entityIdx >= entityToDenseCapacity) return nullptr;

        uint32_t denseIdx = entityToDense[entityIdx];
        if (denseIdx == SENTINEL) return nullptr;

        assert(denseIdx < denseSize);
        return denseBytes + size_t(denseIdx * componentSize);
    }

    void erase(uint32_t entityIdx)
    {
        assert(entityIdx < entityToDenseCapacity);
        assert(denseSize > 0);

        uint32_t denseIdx = entityToDense[entityIdx];
        assert(denseIdx != SENTINEL);
        assert(denseIdx < denseSize);

        uint32_t lastDenseIdx  = --denseSize;
        uint32_t lastEntityIdx = denseToEntity[lastDenseIdx];

        // Mark removed
        entityToDense[entityIdx] = SENTINEL;

        if (denseIdx != lastDenseIdx) {
            // Move last element into the hole
            void* dst = denseBytes + size_t(denseIdx) * componentSize;
            void* src = denseBytes + size_t(lastDenseIdx) * componentSize;
            std::memcpy(dst, src, componentSize);

            denseToEntity[denseIdx] = lastEntityIdx;
            entityToDense[lastEntityIdx] = denseIdx;
        }
    }

    // TODO: We could keep dense and denseToEntity in one block 
    // TODO: Allocate so that things align with Cache line 
    // TODO: Memset only the new bytes. 
    void growDense() {
        size_t need = size_t(denseSize) + 1;
        size_t newCapacity = SnakeMath::roundUpMultiplePow2(need, MEM_CHUNK_SIZE);
        void *newDenseBytes = malloc(newCapacity * componentSize);
        if (!newDenseBytes)
            throw std::bad_alloc();
        void *newDenseToEntity = malloc(newCapacity * sizeof(uint32_t));
        if (!newDenseToEntity)
            throw std::bad_alloc();

        // --- Initialize memory ---
        std::memset(newDenseBytes, 0xFF, newCapacity * componentSize);
        std::memset(newDenseToEntity, 0xFF, newCapacity * sizeof(uint32_t));

        // --- Copy over data ---
        if (denseBytes)
            std::memcpy(newDenseBytes, denseBytes, denseSize * componentSize);
        if (denseToEntity)
            std::memcpy(newDenseToEntity, denseToEntity, denseSize * sizeof(uint32_t));

        // --- Free ---
        if (denseBytes)
            free(denseBytes);
        if (denseToEntity)
            free(denseToEntity);

        // --- Map ---
        denseBytes = (uint8_t *)newDenseBytes;
        denseToEntity = (uint32_t *)newDenseToEntity;
        denseCapacity = newCapacity;

        assert(denseCapacity > denseSize);
    }

    void growEntityToDense(uint32_t newIdx)
    {
        size_t newCapacity = SnakeMath::roundUpMultiplePow2(newIdx, MEM_CHUNK_SIZE);
        void *newData = malloc(newCapacity * sizeof(uint32_t));
        if (!newData)
            throw std::bad_alloc();

        // Overwrite new memory
        // Notice: int _Val is OK because SENTINEL is 0xFFFFFFFF. (If SENTINEL ever changes, replace with a loop.)
        std::memset(newData, SENTINEL, newCapacity * sizeof(uint32_t));

        // copy existing elements
        if (entityToDense)
            std::memcpy(newData, entityToDense, entityToDenseCapacity * sizeof(uint32_t));

        if (entityToDense)
            free(entityToDense);

        entityToDense = (uint32_t *)newData;
        entityToDenseCapacity = newCapacity;
    }
};

struct EntityManager
{
    // Entity generations
    std::vector<uint8_t> generations;  // generation per slot
    std::vector<uint32_t> freeIndices; // pool of free slots

    // Each component's storage
    ComponentStorage *components[(size_t)ComponentId::COUNT];
    
    // Spatial storage of entities
    ankerl::unordered_dense::map<int64_t, Chunk> chunks;
    
    // All entites that are currently active
    std::vector<Entity> activeEntities;
    
    EntityManager()
    {
        components[(size_t)ComponentId::AABB] = new ComponentStorage(ComponentId::AABB, sizeof(AABB));
        components[(size_t)ComponentId::EntityType] = new ComponentStorage(ComponentId::EntityType, sizeof(EntityType));
        components[(size_t)ComponentId::Health] = new ComponentStorage(ComponentId::Health, sizeof(Health));
        components[(size_t)ComponentId::Material] = new ComponentStorage(ComponentId::Material, sizeof(Material));
        components[(size_t)ComponentId::Mesh] = new ComponentStorage(ComponentId::Mesh, sizeof(Mesh));
        components[(size_t)ComponentId::Renderable] = new ComponentStorage(ComponentId::Renderable, sizeof(Renderable));
        components[(size_t)ComponentId::Transform] = new ComponentStorage(ComponentId::Transform, sizeof(Transform));
        components[(size_t)ComponentId::UvTransform] = new ComponentStorage(ComponentId::UvTransform, sizeof(glm::vec4));
        components[(size_t)ComponentId::Ground] = new ComponentStorage(ComponentId::Ground, sizeof(Ground));
        components[(size_t)ComponentId::GroundCosmetic] = new ComponentStorage(ComponentId::GroundCosmetic, sizeof(GroundCosmetic));
        components[(size_t)ComponentId::GroundOre] = new ComponentStorage(ComponentId::GroundOre, sizeof(GroundOre));
    }

    void push(ComponentId componentId, Entity entity, const void* item) {
        uint8_t *saved = components[(size_t)componentId]->push(item, entityIndex(entity));
        assert(saved != nullptr);
    }

    uint8_t *find(ComponentId componentId, Entity entity) {
        return components[(size_t)componentId]->find(entityIndex(entity));
    }

    void erase(ComponentId componentId, Entity entity) {
        components[(size_t)componentId]->erase(entityIndex(entity));
    }

    Entity createEntity(
        Transform transform,
        Mesh mesh,
        Material material,
        RenderLayer renderLayer,
        EntityType entityType,
        const SpatialStorage &spatialStorage,
        glm::vec4 uvTransform = glm::vec4{},
        uint16_t z = 0)
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
        renderable.packDrawKey(material.shaderType, mesh.vertexOffset);
        AABB aabb = computeWorldAABB(mesh, transform);

        push(ComponentId::Transform, entity, &transform);
        push(ComponentId::Mesh, entity, &mesh);
        push(ComponentId::Renderable, entity, &renderable);
        push(ComponentId::Material, entity, &material);
        push(ComponentId::UvTransform, entity, &uvTransform);
        push(ComponentId::EntityType, entity, &entityType);
        push(ComponentId::AABB, entity, &aabb);

        return entity;
    }

    void destroyEntity(Entity e, const SpatialStorage &spatialStorage)
    {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        uint32_t entityIdx = entityIndex(e);
        uint8_t gen = entityGen(e);

        if (entityIdx >= generations.size())
            return;

        // Safety: only recycle if it’s actually the right generation
        if (generations[entityIdx] == gen)
        {
            uint8_t& g = generations[entityIdx];
            g = uint8_t(g + 1);
            if (g == 0) g = 1;
            
            freeIndices.push_back(entityIdx);
        }

        // --- Remove from spatial storage ---
        switch (spatialStorage)
        {
        case SpatialStorage::Chunk:
        {
            AABB *aabb = (AABB*)find(ComponentId::AABB, e);
            deleteEntityFromChunk(entityIdx, *aabb);
            break;
        }
        case SpatialStorage::ChunkTile:
        {
            AABB *aabb = (AABB*)find(ComponentId::AABB, e);
            deleteEntityFromChunkTile(*aabb);
            break;
        }
        }

        // --- Remove from stores ---
        {
            #ifdef _DEBUG
            ZoneScopedN("Remove from stores");
            #endif

            erase(ComponentId::Transform, e);
            erase(ComponentId::Mesh, e);
            erase(ComponentId::Material, e);
            erase(ComponentId::UvTransform, e);
            erase(ComponentId::EntityType, e);
            erase(ComponentId::AABB, e);
            if (find(ComponentId::Health, e)) erase(ComponentId::Health, e);
            if (find(ComponentId::GroundCosmetic, e)) erase(ComponentId::GroundCosmetic, e);
            if (find(ComponentId::GroundOre, e)) erase(ComponentId::GroundOre, e);
        }
    }

    bool isAlive(Entity &e) const
    {
        uint32_t index = entityIndex(e);
        uint8_t gen = entityGen(e);
        return generations[index] == gen;
    }

    void deleteEntityFromChunk(uint32_t &entityIdx, const AABB &aabb)
    {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

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
        #ifdef _DEBUG
        ZoneScoped;
        #endif

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

    // TODO: Reimplement
    // void collectChunkDebugInstances(std::vector<InstanceData> &instances)
    // {
    //     for (auto &[key, val] : chunks)
    //     {
    //         Transform t = Transform(glm::vec2{val.chunkX, val.chunkY}, glm::vec2{CHUNK_WORLD_SIZE, CHUNK_WORLD_SIZE});
    //         glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 0.25f); // translucent red
    //         InstanceData instanceData(
    //             t.model,
    //             color,
    //             glm::vec4(0, 0, 1, 1),
    //             glm::vec2{0.0f, 0.0f},
    //             glm::vec2{0.0f, 0.0f},
    //             RenderLayer::World,
    //             ShaderType::FlatColor,
    //             0.0f,
    //             0,
    //             MeshRegistry::quad,
    //             AtlasIndex::Sprite,
    //             UINT64_MAX,);
    //         instances.push_back(instanceData);
    //     }
    // }

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
