#pragma once
#include "Engine.h"
#include "Window.h"
#include "Vertex.h"
#include "MeshRegistry.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Collision.h"
#include "TextureComponent.h"
#include "Colors.h"
#include "Transform.h"
#include "SnakeMath.h"
#include "Health.h"
#include "CaveSystem.h"
#include "EntityType.h"
#include "Material.h"
#include "Chunk.h"
#include "Atlas.h"

#define MINIAUDIO_IMPLEMENTATION
#include "../libs/miniaudio.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <filesystem>
#include <cstdlib>
#include <iostream>

// PROFILING
#include "tracy/Tracy.hpp"

struct Player
{
    std::array<Entity, 4> entities;
};

struct Background
{
    Entity entity;
};

inline void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

struct Game
{
    // Core engine + window
    Window window;
    Engine engine;

    // Timing
    uint32_t frameCount = 0;
    double lastTime = 0.0;
    float fpsTimeSum = 0.0f;
    float fps = 0.0f;

    bool gameOver = false;
    Background background;
    Player player;
    Camera camera{1, 1};
    CaveSystem caveSystem;

    // -- Player ---
    const float thrustPower = 18000.0f; // pixels per second squared
    // const float thrustPower = 1800.0f; // TODO REVERT THIS
    const float friction = 4.0f; // friction coefficient
    const uint32_t snakeSize = 32;

    glm::vec2 playerVelocity = {0.0f, 0.0f};
    float rotationSpeed = 5.0f;        // tweak this
    float playerMaxVelocity = 2200.0f; // TODO REVERT THIS
    // float playerMaxVelocity = 1200.0f; // tweak this

    // Rotation
    float rotationRadius = 75.0f;       // Increase to stop earlier
    float maxDistance = rotationRadius; // Increase to stop earlier

    // Engine revs
    float lowRev = 0.75;
    float highRev = 1.25;

    // Drilling
    bool drilling = false;

    // Audio
    ma_engine audioEngine;
    ma_sound engineIdleAudio;

    // Timing
    double particleTimer = 0.0f;
    const double PARTICLE_SPAWN_INTERVAL = 0.2f;

    Game(Window &w) : window(w), engine(w.width, w.height, w), caveSystem(engine) {}

    // --- Lifecycle ---
    void init()
    {
        ZoneScoped; // PROFILER

        try
        {
            Logrador::info(std::filesystem::current_path().string());
            engine.init();
            ma_result result = ma_engine_init(NULL, &audioEngine);
            if (result != MA_SUCCESS)
            {
                throw std::runtime_error("Failed to start audio engine");
            }

            // --- Background ---
            {
                AtlasRegion region = engine.atlasRegions[SpriteID::SPR_CAVE_BACKGROUND];
                Transform t = Transform{glm::vec2{0.0f, 0.0f}, glm::vec2{0.0f, 0.0f}};
                Material material = Material(ShaderType::TextureParallax, AtlasIndex::Sprite, {64.0f, 64.0f});
                glm::vec4 uvTransform = getUvTransform(region);
                t.commit();

                background = {engine.ecs.createEntity(t, MeshRegistry::quad, material, RenderLayer::Background, EntityType::Background, SpatialStorage::Global, uvTransform, 0.0f)};
            }

            // ---- Player ----
            {
                // Start off center map above the grass
                glm::vec2 posCursor = glm::vec2{0.0f, 0.0f};
                {
                    Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::TextureScrolling, AtlasIndex::Sprite, {32.0f, 32.0f}};
                    Transform t = Transform{posCursor, glm::vec2{snakeSize, snakeSize}, "player"};
                    AtlasRegion region = engine.atlasRegions[SpriteID::SPR_DRILL_HEAD];
                    glm::vec4 uvTransform = getUvTransform(region);

                    player.entities[0] = engine.ecs.createEntity(t, MeshRegistry::triangle, m, RenderLayer::World, EntityType::Player, SpatialStorage::Global, uvTransform, 2.0f);
                }

                AtlasRegion region = engine.atlasRegions[SpriteID::SPR_SNAKE_SKIN];
                glm::vec4 uvTransform = getUvTransform(region);
                for (size_t i = 1; i < 4; i++)
                {
                    posCursor -= glm::vec2{snakeSize, 0.0f};
                    Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite, {32.0f, 32.0f}};
                    Transform t = Transform{posCursor, glm::vec2{snakeSize, snakeSize}, "player"};
                    Entity entity = engine.ecs.createEntity(t, MeshRegistry::quad, m, RenderLayer::World, EntityType::Player, SpatialStorage::Global, uvTransform, 2.0f);
                    player.entities[i] = entity;
                }
            }
            // --- Camera ---
            camera = Camera{window.width, window.height};

            // --- Ground ----
            caveSystem.createGraceArea();

            engine.ecs.sortRenderables();

            // --- AUDIO ---
            ma_engine_set_volume(&audioEngine, 0.025);
            ma_sound_init_from_file(&audioEngine, "assets/engine_idle.wav", 0, NULL, NULL, &engineIdleAudio);
            ma_sound_set_looping(&engineIdleAudio, MA_TRUE);
            ma_sound_start(&engineIdleAudio);

            // --- Scrolling ---
            glfwSetWindowUserPointer(window.handle, this); // Connects this instance of struct to the windows somehow
            glfwSetScrollCallback(window.handle, scrollCallback);
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string("Failed to init game: ") + e.what());
        }
        catch (...)
        {
            throw std::runtime_error(std::string("Unknown exception thrown in Game::init"));
        }
    }

    void
    run()
    {
        Logrador::info("Starting game loop");

        tracy::SetThreadName("MainThread"); // Optional, nice for visualization

        while (!window.shouldClose())
        {
            ZoneScoped; // PROFILER

            window.pollEvents();

            double currentTime = glfwGetTime();
            double delta = currentTime - lastTime;
            lastTime = currentTime;

            if (gameOver)
                throw std::runtime_error("You fucked up");

            updateTimers(delta);

            updateGame(delta);

            updateEngineRevs();

            updateCamera();

            updateLifecycle();

            engine.globalTime += delta;

            Transform &head = engine.ecs.transforms[engine.ecs.entityToTransform[entityIndex(player.entities.front())]];
            engine.draw(camera, fps, head.position, delta);

            updateFPSCounter(delta);
        }

        ma_sound_uninit(&engineIdleAudio);
        ma_engine_uninit(&audioEngine);
    }

    // --- Game logic ---
    void updateWorldGen()
    {
        ZoneScoped; // PROFILER

        // When player moves into a new chunk we should verify that there are in fact 3x3 loaded chunks around the player
        Transform &head = engine.ecs.transforms[engine.ecs.entityToTransform[entityIndex(player.entities.front())]];
        int32_t cx = worldPosToClosestChunk(head.position.x);
        int32_t cy = worldPosToClosestChunk(head.position.y);

        for (int dx = -2; dx <= 2; dx++)
        {
            for (int dy = -2; dy <= 2; dy++)
            {
                int32_t chunkWorldX = cx + dx * CHUNK_WORLD_SIZE;
                int32_t chunkWorldY = cy + dy * CHUNK_WORLD_SIZE;
                int64_t chunkIdx = packChunkCoords(chunkWorldX, chunkWorldY);

                if (engine.ecs.chunks.find(chunkIdx) == engine.ecs.chunks.end())
                {
                    caveSystem.generateNewChunk(chunkWorldX, chunkWorldY);
                }
            }
        }
    }

    void updateLifecycle()
    {
        ZoneScoped;

        updateWorldGen();

        // Clear first
        engine.ecs.activeEntities.clear();

        // Find all active
        float halfW = (camera.screenW * 0.5f) / camera.zoom;
        float halfH = (camera.screenH * 0.5f) / camera.zoom;
        AABB cameraBox{
            {camera.position.x - halfW, camera.position.y - halfH},
            {camera.position.x + halfW, camera.position.y + halfW},
        };
        size_t playerIndexT = engine.ecs.entityToTransform[entityIndex(player.entities.front())];
        Transform playerT = engine.ecs.transforms[playerIndexT];
        engine.ecs.getAllRelevantEntities(playerT, engine.ecs.activeEntities);

        size_t writeIndex = 0;
        size_t readIndex = 0;
        for (; readIndex < engine.ecs.activeEntities.size(); ++readIndex)
        {
            ZoneScopedN("updateLifeCycleEntity");

            Entity entity = engine.ecs.activeEntities[readIndex];
            uint32_t entityIdx = entityIndex(entity);
            uint32_t &entityTypeIndex = engine.ecs.entityToEntityTypes[entityIdx];
            if (entityTypeIndex == UINT32_MAX)
            {
                printf("Skipping %d\n", entityIdx);
                engine.ecs.activeEntities[writeIndex++] = entity;
                continue;
            }

            switch (engine.ecs.entityTypes[entityTypeIndex])
            {
            case EntityType::Ground:
            {
                Health &health = engine.ecs.healths[engine.ecs.entityToHealth[entityIdx]];
                Material &material = engine.ecs.materials[engine.ecs.entityToMaterial[entityIdx]];
                material.color.a = health.current / health.max;

                // Check if ground block has died
                if (health.current > 0)
                {
                    engine.ecs.activeEntities[writeIndex++] = entity;
                    break;
                }

                engine.ecs.destroyEntity(entity, SpatialStorage::ChunkTile);
                break;
            }
            case EntityType::Treasure:
            {
                uint32_t &treasureIndex = engine.ecs.entityToTreasure[entityIdx];
                if (treasureIndex == UINT32_MAX)
                {
                    engine.ecs.activeEntities[writeIndex++] = entity;
                    break;
                }

                Treasure &treasure = engine.ecs.treasures[treasureIndex];

                // Dies if it has lost it's ground
                if (treasure.groundRef.has_value() && engine.ecs.isAlive(treasure.groundRef.value()))
                {
                    engine.ecs.activeEntities[writeIndex++] = entity;
                    break;
                }

                engine.ecs.destroyEntity(entity, SpatialStorage::Chunk);
                break;
            }
            default:
                engine.ecs.activeEntities[writeIndex++] = entity;
                break;
            }
        }

        if (writeIndex < readIndex)
            engine.ecs.activeEntities.resize(writeIndex);

        // Add static
        engine.ecs.activeEntities.push_back(background.entity);
    }

    void updateCamera()
    {
        ZoneScoped; // PROFILER

        Entity entity = background.entity;
        size_t playerIndexT = engine.ecs.entityToTransform[entityIndex(player.entities.front())];
        size_t backgroundIndexT = engine.ecs.entityToTransform[entityIndex(entity)];

        Transform &playerTransform = engine.ecs.transforms[playerIndexT];
        Transform &backgroundTransform = engine.ecs.transforms[backgroundIndexT];
        Transform &backgroundTransformOld = backgroundTransform;
        Mesh &mesh = engine.ecs.meshes[engine.ecs.entityToMesh[entityIndex(entity)]];

        // Camera center follows the player
        camera.position = glm::round(playerTransform.position);

        // Compute visible area size, factoring in zoom
        glm::vec2 viewSize = glm::vec2(camera.screenW, camera.screenH) * (1.0f / camera.zoom);

        // Background should cover the whole visible region and be centered on the camera
        backgroundTransform.position = camera.position - viewSize * 0.5f;
        backgroundTransform.size = viewSize;
        backgroundTransform.commit();
        AABB oldAABB = computeWorldAABB(mesh, backgroundTransformOld);
        AABB newAABB = computeWorldAABB(mesh, backgroundTransform);
    }

    void updateTimers(double delta)
    {
        ZoneScoped; // PROFILER

        particleTimer = max(particleTimer - delta, 0.0f);
    }

    void updateGame(double delta)
    {
        ZoneScoped; // PROFILER

        // Constants / parameters
        GLFWwindow *handle = window.handle;

        // --- Rotation ---
        auto change = rotationSpeed * delta;
        bool forwardPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS)
            rotateHeadLeft(delta);
        else if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS)
            rotateHeadRight(delta);

        bool forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
        updateMovement(delta, forward);
    }

    void updateEngineRevs()
    {
        ZoneScoped; // PROFILER
        if (drilling)
        {
            ma_sound_set_pitch(&engineIdleAudio, highRev);
            return;
        }

        Transform &playerTransform = engine.ecs.transforms[entityIndex(player.entities.front())];
        glm::vec2 forward = SnakeMath::getRotationVector2(playerTransform.rotation);
        float velocity = glm::dot(playerVelocity, forward);
        float ratio = velocity / playerMaxVelocity;
        float revs = ((highRev - lowRev) * ratio) + lowRev;

        ma_sound_set_pitch(&engineIdleAudio, revs);
    }

    void tryMove(Transform &head, Mesh &mesh, const glm::vec2 &targetPos, float dt)
    {
        ZoneScoped; // PROFILER

        Transform newTransform = head;
        newTransform.position = targetPos;
        AABB newAABB = computeWorldAABB(mesh, newTransform);

        drilling = false; // We reset drill state because we don't know if there'll be any drilling yet

        for (Entity &entity : engine.ecs.activeEntities)
        {
            size_t collisionBoxIdx = engine.ecs.entityToCollisionBox[entityIndex(entity)];
            if (collisionBoxIdx == UINT32_MAX)
                continue;
            AABB &collisionBox = engine.ecs.collisionBoxes[collisionBoxIdx];
            EntityType &entityType = engine.ecs.entityTypes[engine.ecs.entityToEntityTypes[entityIndex(entity)]];
            if (entityType == EntityType::Player)
                continue;

            float radius = head.getRadius();
            if (circleIntersectsAABB(head.getCenter(), radius, collisionBox))
            {
                switch (entityType)
                {
                case EntityType::Ground:
                {
                    Health &h = engine.ecs.healths[engine.ecs.entityToHealth[entityIndex(entity)]];
                    size_t collisionBoxIndex = engine.ecs.entityToCollisionBox[entityIndex(entity)];
                    float penetration = 0.5f;
                    float resistance = glm::clamp(penetration / 10.0f, 0.0f, 1.0f);

                    // Reduce velocity
                    playerVelocity *= 0.95;

                    // Damage block
                    float damage = 25000.0f * dt;
                    // float damage = 250.0f * dt; TODO REVERT THIS BACK
                    h.current -= damage;
                    drilling = true;
                    break;
                }
                default:
                    break;
                }
            }
        }

        if (drilling)
        {
            glm::vec2 drillTipLocal = MeshRegistry::getDrillTipLocal().pos; // not UV
            glm::vec4 local = glm::vec4(drillTipLocal, 0.0f, 1.0f);
            glm::vec4 world = head.model * local;

            glm::vec2 drillTipWorld = glm::vec2(world.x, world.y);

            if (particleTimer <= 0)
            {
                engine.particleSystem.updateSpawnFlag(drillTipWorld, SnakeMath::getRotationVector2(head.rotation), 8);
                particleTimer = PARTICLE_SPAWN_INTERVAL;
            }
        }
    }

    void updateMovement(float dt, bool pressing)
    {
        ZoneScoped; // PROFILER

        uint32_t headEntityId = entityIndex(player.entities.front());
        auto headIndexT = engine.ecs.entityToTransform[headEntityId];
        Transform &headT = engine.ecs.transforms[headIndexT];
        Transform oldHeadT = headT;
        Mesh &headM = engine.ecs.meshes[engine.ecs.entityToMesh[headEntityId]];
        const Mesh &bodyM = MeshRegistry::quad; // NOTE This might not work in the future

        glm::vec2 acceleration = {0.0f, 0.0f};
        glm::vec2 forward = SnakeMath::getRotationVector2(headT.rotation);
        acceleration = forward * thrustPower;

        // --- Velocity integration ---
        if (pressing)
            playerVelocity += acceleration * dt;
        // Friction
        playerVelocity -= playerVelocity * friction * dt;
        float speed = glm::length(playerVelocity);
        // Max speed
        if (speed > playerMaxVelocity)
            playerVelocity = glm::normalize(playerVelocity) * playerMaxVelocity;

        // Check collision
        glm::vec2 targetPos = headT.position + playerVelocity * dt;
        tryMove(headT, headM, targetPos, dt);
        headT.position += playerVelocity * dt;

        // All non head segments gets to make a move
        for (size_t i = 1; i < player.entities.size(); i++)
        {
            auto entity1 = player.entities[i - 1];
            auto entity2 = player.entities[i];
            auto tIndex1 = engine.ecs.entityToTransform[entityIndex(entity1)];
            auto tIndex2 = engine.ecs.entityToTransform[entityIndex(entity2)];
            Transform &t1 = engine.ecs.transforms[tIndex1];
            Transform &t2 = engine.ecs.transforms[tIndex2];
            Transform &t2Old = t2;
            glm::vec2 prevPos = t1.position;
            glm::vec2 &pos = t2.position;
            glm::vec2 dir = prevPos - pos;

            float dist = glm::length(dir);
            if (dist > snakeSize)
            {
                dir = glm::normalize(dir);
                pos = prevPos - dir * (float)snakeSize;
            }

            // Optionally adjust rotation
            t2.rotation = atan2(dir.y, dir.x);
            t2.commit();
        }

        // Move head
        headT.commit();
    }

    void rotateHeadLeft(float dt)
    {
        ZoneScoped; // PROFILER

        auto tIndex = engine.ecs.entityToTransform[player.entities[2].id];
        Transform &transform = engine.ecs.transforms[tIndex];
        glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);
        glm::vec2 leftDir = glm::vec2(forward.y, -forward.x);
        glm::vec2 radiusCenter = transform.position + leftDir * rotationRadius;

        float rotationSpeed = 5.0f;         // tweak this
        float maxDistance = rotationRadius; // Increase to stop earlier

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i];
            auto segmentIndex = engine.ecs.entityToTransform[entityIndex(entity)];
            Transform &segment = engine.ecs.transforms[segmentIndex];
            glm::vec2 localCenter = segment.position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(segment.rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(localCenter.y, -localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(segment.position - radiusCenter);
            if (dist < maxDistance)
                continue;

            segment.position = segment.position - dt * (localCenter);
            segment.rotation += deltaAngle * dt * rotationSpeed;
            segment.commit();
            break;
        }
    }

    void rotateHeadRight(float dt)
    {
        ZoneScoped; // PROFILER

        auto tIndex = engine.ecs.entityToTransform[player.entities[2].id];
        Transform &transform = engine.ecs.transforms[tIndex];
        glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);
        glm::vec2 rightDir = glm::vec2(-forward.y, forward.x);
        glm::vec2 radiusCenter = transform.position + rightDir * rotationRadius;

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i];
            auto segmentIndex = engine.ecs.entityToTransform[entityIndex(entity)];
            Transform &segment = engine.ecs.transforms[segmentIndex];
            glm::vec2 localCenter = segment.position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(segment.rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(-localCenter.y, localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(segment.position - radiusCenter);
            if (dist < maxDistance)
                continue;

            segment.position = segment.position - dt * (localCenter);
            segment.rotation += deltaAngle * dt * rotationSpeed;
            segment.commit();
            break;
        }
    }

    void updateFPSCounter(float delta)
    {
        ZoneScoped; // PROFILER
        frameCount++;

        if (frameCount >= 400)
        {
            fps = round(frameCount / (fpsTimeSum));
            frameCount = 0;
            fpsTimeSum = 0.0;
        }
        else
        {
            fpsTimeSum += delta;
        }
    }
};

inline void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    ZoneScoped; // PROFILER

    // TODO Add scrollMax before release or maybe max when DEBUG isn't present
    Game *game = reinterpret_cast<Game *>(glfwGetWindowUserPointer(window));
    if (!game)
        return;

    // Apply zoom scaling
    game->camera.zoom *= (1.0f + (float)yoffset * 0.1f);
    game->camera.zoom = glm::clamp(game->camera.zoom, 0.05f, 4.0f);
}