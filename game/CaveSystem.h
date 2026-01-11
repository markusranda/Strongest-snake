#pragma once
#include "components/EntityType.h"
#include "components/Material.h"
#include "components/Health.h"
#include "components/GroundOre.h"
#include "EntityManager.h"
#include "SnakeMath.h"
#include "Atlas.h"
#include "Item.h"
#include "glm/common.hpp"

// PROFILING
#include "tracy/Tracy.hpp"

const char *GROUND_NAME = "ground";
const char *SPRITE_KEY = "ground_mid_1";

struct CaveSystem
{
    EntityManager *ecs;
    RendererInstanceStorage *instanceStorage;
    AtlasRegion *atlasRegions;

    const float tileSize = 32.0f;
    uint32_t lastMapIndex = 19;
    glm::vec2 size = {tileSize, tileSize};
    Material material = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
    static constexpr int TREASURE_COUNT = 10;
    std::array<SpriteID, TREASURE_COUNT> GROUND_COSMETICS = {
        SpriteID::SPR_GEM_BLUE,
        SpriteID::SPR_GEM_RED,
        SpriteID::SPR_GEM_GREEN,
        SpriteID::SPR_GEM_ORANGE,
        SpriteID::SPR_GEM_PURPLE,
        SpriteID::SPR_GEMS_BLUE,
        SpriteID::SPR_GEMS_GREEN,
        SpriteID::SPR_GEMS_PURPLE,
        SpriteID::SPR_GEMS_ORANGE,
        SpriteID::SPR_SKULL,
    };

    float worldToTileCoord(float coord)
    {
        return coord * tileSize;
    }

    void createInstanceData(Entity entity, Transform transform, Material material, Mesh mesh, glm::vec4 uvTransform)
    {
        ZoneScoped;
        Renderable *renderable = (Renderable*)ecs->find(ComponentId::Renderable, entity);
        InstanceData instance = {
            transform.model,
            material.color,
            uvTransform,
            transform.size,
            material.size,
            renderable->renderLayer,
            material.shaderType,
            renderable->z,
            renderable->tiebreak,
            mesh,
            material.atlasIndex,
            renderable->drawkey,
            entity,
        };

        instanceStorage->push(instance);
    }
    
    Entity createGroundCosmetic(Entity &groundEntity, Transform &transform, uint32_t key)
    {
        Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
        AtlasRegion region = atlasRegions[key];
        glm::vec4 uvTransform = getUvTransform(region);
        Mesh mesh = MeshRegistry::quad;
        Entity entity = ecs->createEntity(
            transform,
            mesh,
            m,
            RenderLayer::World,
            EntityType::GroundCosmetic,
            SpatialStorage::Chunk,
            uvTransform,
            1);
        GroundCosmetic groundCosmetic = {groundEntity};
        ecs->push(ComponentId::GroundCosmetic, entity, &groundCosmetic);

        return entity;
    }

    Entity createGroundOre(Entity &groundEntity, Transform &transform, OreDef orePackage)
    {
        Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
        AtlasRegion region = atlasRegions[orePackage.spriteID];
        glm::vec4 uvTransform = getUvTransform(region);
        Mesh mesh = MeshRegistry::quad;
        Entity entity = ecs->createEntity(
            transform,
            mesh,
            m,
            RenderLayer::World,
            EntityType::OreBlock,
            SpatialStorage::Chunk,
            uvTransform,
            1);
        GroundOre groundOre = { .itemId = orePackage.itemId, .parentRef = groundEntity, .oreLevel = orePackage.level  };
        ecs->push(ComponentId::GroundOre, entity, &groundOre);

        return entity;
    }

    Entity createRandomGroundCosmetic(Entity &groundEntity, Transform &t)
    {
        int idx = std::lround(SnakeMath::randomBetween(0, TREASURE_COUNT - 1));
        uint32_t key = GROUND_COSMETICS[idx];
        return createGroundCosmetic(groundEntity, t, key);
    }

    Entity createRandomOreBlock(Entity &groundEntity, Transform &transform)
    {
        // TODO: This should be based on something more interesting like "depth" or "biome"
        ItemId idx = (ItemId)std::lround(SnakeMath::randomBetween(0, oreDatabase.count - 1));
        OreDef orePackage = oreDatabase[idx];
        return createGroundOre(groundEntity, transform, orePackage);
    }

    inline void createGround(float xWorld, float yWorld)
    {
        ZoneScoped;

        glm::vec2 position = {xWorld, yWorld};
        Transform transform = {position, size, GROUND_NAME};
        AtlasRegion region = atlasRegions[SpriteID::SPR_GROUND_MID_1];
        glm::vec4 uvTransform = getUvTransform(region);
        Mesh mesh = MeshRegistry::quad;
        Ground ground = Ground{};
        Entity entity = ecs->createEntity(
            transform,
            mesh,
            material,
            RenderLayer::World,
            EntityType::Ground,
            SpatialStorage::ChunkTile,
            uvTransform,
            0);

        if (SnakeMath::chance(0.005)) {
            createRandomGroundCosmetic(entity, transform);
        } else if (SnakeMath::chance(0.005)) {
            ground.groundOreRef = createRandomOreBlock(entity, transform);
        }
        
        Health health = Health{100, 100};
        ecs->push(ComponentId::Health, entity, &health);
        ecs->push(ComponentId::Ground, entity, &ground);
    }

    inline void createGraceArea()
    {
        ZoneScoped;

        float xMin = -1 * (float)(2 * CHUNK_WORLD_SIZE);
        float xMax = (float)(3 * CHUNK_WORLD_SIZE);
        float yMin = -1 * (float)(2 * CHUNK_WORLD_SIZE);
        float yMax = (float)(3 * CHUNK_WORLD_SIZE);
        float cx = 0.0f;
        float cy = 0.0f;
        float radiusInner = 512.0f;

        for (int dx = -2; dx <= 2; dx++)
        {
            for (int dy = -2; dy <= 2; dy++)
            {
                int32_t cx = dx * CHUNK_WORLD_SIZE;
                int32_t cy = dy * CHUNK_WORLD_SIZE;

                ecs->chunks.emplace(
                    packChunkCoords(cx, cy),
                    Chunk{cx, cy});
            }
        }

        for (float y = yMin; y < yMax; y += tileSize)
        {
            for (float x = xMin; x < xMax; x += tileSize)
            {
                float dx = (x - cx) / 1.3f;
                float dy = (y - cy) / 0.8f;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist < radiusInner)
                    continue;

                createGround(x, y);
            }
        }
    }

    void generateNewChunk(int64_t &chunkIdx, int32_t &chunkWorldX, int32_t &chunkWorldY)
    {
        ZoneScoped;

        ecs->chunks.emplace(chunkIdx, Chunk{chunkWorldX, chunkWorldY});
        for (int32_t y = chunkWorldY; y < chunkWorldY + CHUNK_WORLD_SIZE; y += tileSize)
        {
            int32_t xMax = chunkWorldX + CHUNK_WORLD_SIZE;
            for (int32_t x = chunkWorldX; x < xMax; x += tileSize)
            {
                createGround(x, y);
            }
        }
    }
};