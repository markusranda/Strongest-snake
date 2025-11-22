#pragma once

#include "EntityManager.h"
#include "SnakeMath.h"
#include "Health.h"
#include "glm/common.hpp"

// PROFILING
#include "tracy/Tracy.hpp"

struct CaveSystem
{
    const float tileSize = 32.0f;
    uint32_t lastMapIndex = 19;
    glm::vec2 size = {tileSize + 1.0f, tileSize + 1.0f}; // Added 1 pixel in both height and width to remove line artifacts
    Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
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
        // uint32_t spriteIndex = glm::clamp((uint32_t)floor(radius * lastMapIndex), (uint32_t)1, lastMapIndex);
        uint32_t spriteIndex = 1; // TODO Be more clever about the sprites being used
        std::string key = "ground_mid_" + std::to_string(spriteIndex);
        if (engine.atlasRegions.find(key) == engine.atlasRegions.end())
            throw std::runtime_error("you cocked up the ground tiles somehow");
        AtlasRegion region = engine.atlasRegions[key];
        Transform &t = Transform{{xWorld, yWorld}, size, "ground"};
        glm::vec4 uvTransform = getUvTransform(region);
        Entity entity = engine.ecs.createEntity(t, MeshRegistry::quad, m, RenderLayer::World, EntityType::Ground, uvTransform, 8.0f);
        Health h = Health{100, 100};

        if (SnakeMath::chance(0.005))
        {
            createRandomTreasure(entity, t);
        }

        engine.ecs.addToStore(engine.ecs.healths, engine.ecs.entityToHealth, engine.ecs.healthToEntity, entity, h);
    }

    inline void createGraceArea()
    {
        // Top left corner of grace area
        float xMin = -2048.0f;
        float xMax = 2048.0f;
        float yMin = -2048.0f;
        float yMax = 2048.0f;
        float cx = 0.0f;
        float cy = 0.0f;
        float radiusInner = 512.0f;

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
};