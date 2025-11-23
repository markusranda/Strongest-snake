#pragma once
#include "EntityType.h"
#include "EntityManager.h"
#include "SnakeMath.h"
#include "Health.h"
#include "Material.h"
#include "glm/common.hpp"

// PROFILING
#include "tracy/Tracy.hpp"

struct CaveSystem
{
    const float tileSize = 32.0f;
    uint32_t lastMapIndex = 19;
    glm::vec2 size = {tileSize + 1.0f, tileSize + 1.0f}; // Added 1 pixel in both height and width to remove line artifacts
    Material material = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
    static constexpr int TREASURE_COUNT = 10;
    std::array<const char *, TREASURE_COUNT> ground_treasures = {
        "gem_blue",
        "gem_red",
        "gem_green",
        "gem_orange",
        "gem_purple",
        "gems_blue",
        "gems_green",
        "gems_purple",
        "gems_orange",
        "skull",
    };
    Engine &engine;

    CaveSystem(Engine &engine) : engine(engine) {}

    float worldToTileCoord(float coord)
    {
        return coord * tileSize;
    }

    void createRandomTreasure(Entity &groundEntity, Transform &t)
    {
        int idx = std::lround(SnakeMath::randomBetween(0, TREASURE_COUNT - 1));
        std::string treasureKey = ground_treasures[idx];
        createTreasure(groundEntity, t, treasureKey);
    }

    void createTreasure(Entity &groundEntity, Transform &t, std::string key)
    {
        Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
        AtlasRegion region = engine.atlasRegions[key];
        glm::vec4 uvTransform = getUvTransform(region);
        Entity treasureEntity = engine.ecs.createEntity(t, MeshRegistry::quad, m, RenderLayer::World, EntityType::Treasure, uvTransform, 0.0f);
        Treasure treasure = {groundEntity};
        engine.ecs.addToStore(engine.ecs.treasures, engine.ecs.entityToTreasure, engine.ecs.treasureToEntity, treasureEntity, treasure);
    }

    inline void createGround(float xWorld, float yWorld)
    {
        ZoneScoped;

        std::string key = "ground_mid_1";
        assert(engine.atlasRegions.find(key) != engine.atlasRegions.end());
        Transform &transform = Transform{{xWorld, yWorld}, size, "ground"};
        Entity entity = engine.ecs.createChunkEntity(
            transform,
            MeshRegistry::quad,
            material,
            RenderLayer::World,
            EntityType::Ground,
            getUvTransform(engine.atlasRegions[key]),
            8.0f);

        if (SnakeMath::chance(0.005))
        {
            createRandomTreasure(entity, transform);
        }

        engine.ecs.addToStore(engine.ecs.healths, engine.ecs.entityToHealth, engine.ecs.healthToEntity, entity, Health{100, 100});
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

                engine.ecs.chunks.emplace(
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

    void generateNewChunk(int32_t chunkWorldX, int32_t chunkWorldY)
    {
        engine.ecs.chunks.emplace(packChunkCoords(chunkWorldX, chunkWorldY), Chunk{chunkWorldX, chunkWorldY});
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