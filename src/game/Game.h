#pragma once
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/Vertex.h"
#include "engine/MeshRegistry.h"
#include "engine/EntityManager.h"
#include "engine/Camera.h"
#include "engine/Collision.h"
#include "engine/TextureComponent.h"
#include "Colors.h"
#include "engine/Transform.h"
#include "SnakeMath.h"

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

struct Ground
{
    Entity entity;
    float health = 100;
    float maxHealth = 100;
    bool dead = false;
    bool dirty = false;
};

struct Player
{
    std::array<Entity, 4> entities;
};
struct Background
{
    Entity entity;
};

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
    std::unordered_map<Entity, Ground> grounds;

    // Game settings
    const uint32_t rows = 1000;
    const uint32_t columns = 20;
    const uint32_t groundSize = 32;

    // -- Player ---
    const float thrustPower = 800.0f; // pixels per second squared
    const float friction = 4.0f;      // friction coefficient
    const uint32_t snakeSize = 32;

    glm::vec2 playerVelocity = {0.0f, 0.0f};
    float rotationSpeed = 5.0f;       // tweak this
    float playerMaxVelocity = 200.0f; // tweak this

    // Rotation
    float rotationRadius = 75.0f;       // Increase to stop earlier
    float maxDistance = rotationRadius; // Increase to stop earlier

    // Engine revs
    float lowRev = 0.75;
    float highRev = 1.25;

    // Audio
    ma_engine audioEngine;
    ma_sound engineIdleAudio;

    Game(Window &w)
        : window(w),
          engine(w.width, w.height, w)
    {
    }

    // --- Lifecycle ---
    void init()
    {
        try
        {
            Logrador::info(std::filesystem::current_path().string());
            engine.init("assets/atlas.png", "assets/fonts.png", "assets/atlas.rigdb");
            ma_result result = ma_engine_init(NULL, &audioEngine);
            if (result != MA_SUCCESS)
            {
                throw std::runtime_error("Failed to start audio engine");
            }

            // --- Background ---
            {
                Transform transform = Transform{glm::vec2{0, 0}, glm::vec2{window.width, window.height}};
                Material material = Material{Colors::fromHex(Colors::SKY_BLUE, 1.0f), ShaderType::FlatColor, AtlasIndex::Sprite};
                background = {engine.ecs.createEntity(transform, MeshRegistry::quad, material, RenderLayer::Background, EntityType::Background)};
            }

            // ---- Player ----
            {
                // Start off center map above the grass
                glm::vec2 posCursor = glm::vec2{(columns / 2) * 32.0f, -32.0f};
                {
                    Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::TextureScrolling, AtlasIndex::Sprite};
                    Transform t = Transform{posCursor, glm::vec2{snakeSize, snakeSize}, "player"};
                    AtlasRegion region = engine.atlasRegions["drill_head"];
                    glm::vec4 uvTransform = getUvTransform(region);

                    player.entities[0] = engine.ecs.createEntity(t, MeshRegistry::triangle, m, RenderLayer::World, EntityType::Player, uvTransform, true);
                }

                AtlasRegion region = engine.atlasRegions["snake_skin"];
                glm::vec4 uvTransform = getUvTransform(region);
                for (size_t i = 1; i < 4; i++)
                {
                    posCursor -= glm::vec2{snakeSize, 0.0f};
                    Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite};
                    Transform t = Transform{posCursor, glm::vec2{snakeSize, snakeSize}, "player"};
                    Entity entity = engine.ecs.createEntity(t, MeshRegistry::quad, m, RenderLayer::World, EntityType::Player, uvTransform);
                    player.entities[i] = entity;
                }
            }
            // --- Camera ---
            camera = Camera{window.width, window.height};

            // --- Ground ----
            {
                Material m = Material{Colors::fromHex(Colors::WHITE, 1.0f), ShaderType::Texture, AtlasIndex::Sprite};
                uint32_t lastMapIndex = 19;
                for (uint32_t y = 0; y < rows; y++)
                {
                    for (uint32_t x = 0; x < columns; x++)
                    {
                        AtlasRegion region;
                        float ratio = (float)y / (float)rows;
                        uint32_t spriteIndex = glm::clamp((uint32_t)floor(ratio * lastMapIndex), (uint32_t)1, lastMapIndex);
                        std::string key = "ground_mid_" + std::to_string(spriteIndex);

                        // Special case first is always the grassy grass
                        if (y == 0)
                            key = "ground_mid_0";

                        if (engine.atlasRegions.find(key) == engine.atlasRegions.end())
                            throw std::runtime_error("you cocked up the ground tiles somehow");
                        region = engine.atlasRegions[key];

                        Transform t = Transform{{x * groundSize, y * groundSize}, {groundSize, groundSize}, "ground"};
                        glm::vec4 uvTransform = getUvTransform(region);

                        Entity entity = engine.ecs.createEntity(t, MeshRegistry::quad, m, RenderLayer::World, EntityType::Ground, uvTransform, true);
                        grounds.insert_or_assign(entity, Ground{entity, 100});
                    }
                }
            }

            // --- AUDIO ---
            ma_engine_set_volume(&audioEngine, 0.025);
            ma_sound_init_from_file(&audioEngine, "assets/engine_idle.wav", 0, NULL, NULL, &engineIdleAudio);
            ma_sound_set_looping(&engineIdleAudio, MA_TRUE);
            ma_sound_start(&engineIdleAudio);
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

    void run()
    {
        Logrador::info("Starting game loop");

        tracy::SetThreadName("MainThread"); // Optional, nice for visualization

        while (!window.shouldClose())
        {
            ZoneScoped; // PROFILER

            window.pollEvents();

            double currentTime = glfwGetTime() * 1000.0;
            double delta = currentTime - lastTime;
            lastTime = currentTime;

            if (gameOver)
                throw std::runtime_error("You fucked up");

            updateGame(delta);

            updateEngineRevs();

            updateCamera();

            updateLifecycle();

            engine.globalTime += delta;

            engine.draw(camera, fps);

            updateFPSCounter(delta);
        }

        ma_sound_uninit(&engineIdleAudio);
        ma_engine_uninit(&audioEngine);
    }

    // --- Game logic ---
    void updateLifecycle()
    {
        ZoneScoped; // PROFILER
        std::vector<Entity> deadGrounds;
        for (auto &[entity, ground] : grounds)
        {
            if (ground.dirty)
            {
                Material &m = engine.ecs.materials[engine.ecs.entityToMaterial[entityIndex(ground.entity)]];
                m.color.a = ground.health / ground.maxHealth;
                if (ground.health <= 0)
                {
                    ground.dead = true;
                }

                if (ground.dead)
                {
                    deadGrounds.push_back(entity);
                }

                ground.dirty = false;
            }
        }

        for (auto &entity : deadGrounds)
        {
            grounds.erase(entity);
            engine.ecs.destroyEntity(entity);
        }
    }

    void updateCamera()
    {
        ZoneScoped; // PROFILER
        size_t playerIndexT = engine.ecs.entityToTransform[entityIndex(player.entities.front())];
        size_t backgroundIndexT = engine.ecs.entityToTransform[entityIndex(background.entity)];

        Transform &playerTransform = engine.ecs.transforms[playerIndexT];
        Transform &backgroundTransform = engine.ecs.transforms[backgroundIndexT];

        camera.position = playerTransform.position - glm::vec2(window.width / 2.0f, window.height / 2.0f);
        backgroundTransform.position = camera.position;
        backgroundTransform.commit();
    }

    void updateGame(double delta)
    {
        ZoneScoped; // PROFILER

        // Constants / parameters
        const float dt = static_cast<float>(delta) / 1000.0f;
        GLFWwindow *handle = window.getHandle();

        // --- Rotation ---
        auto change = rotationSpeed * dt;
        bool forwardPressed = false;
        bool leftPressed = false;
        bool rightPressed = false;

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS)
            rotateHeadLeft(dt);
        else if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS)
            rotateHeadRight(dt);

        bool forward = glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS;
        updateMovement(dt, forward);
    }

    void updateEngineRevs()
    {
        Transform &playerTransform = engine.ecs.transforms[entityIndex(player.entities.front())];
        glm::vec2 forward = SnakeMath::getRotationVector2(playerTransform.rotation);
        float velocity = glm::dot(playerVelocity, forward);
        float ratio = velocity / playerMaxVelocity;
        float revs = ((highRev - lowRev) * ratio) + lowRev;

        ma_sound_set_pitch(&engineIdleAudio, revs);
    }

    std::vector<Entity> getAllCollisions(Transform &t)
    {
        std::vector<Entity> entities{};
        for (size_t i = 0; i < engine.ecs.collisionBoxes.size(); i++)
        {
            AABB &collisionBox = engine.ecs.collisionBoxes[i];
            Entity collisionEntity = engine.ecs.getEntityFromDense(i, engine.ecs.collisionBoxToEntity);
            EntityType entityType = engine.ecs.entityTypes[engine.ecs.entityToEntityTypes[entityIndex(collisionEntity)]];
            if (entityType == EntityType::Player)
                continue;

            float radius = t.getRadius() * 0.5f;
            if (circleIntersectsAABB(t.getCenter(), radius, collisionBox) && entityType == EntityType::Ground)
            {
                entities.push_back(collisionEntity);
            }
        }

        return entities;
    }

    void tryMove(Transform &head, Mesh &mesh, const glm::vec2 &targetPos, float dt)
    {
        Transform newTransform = head;
        newTransform.position = targetPos;

        std::vector<Entity> collisions = getAllCollisions(newTransform);
        if (collisions.size() < 1)
            return;

        // simulate "digging" resistance
        for (Entity collisionEntity : collisions)
        {
            Ground &g = grounds[collisionEntity];
            size_t collisionBoxIndex = engine.ecs.entityToCollisionBox[entityIndex(collisionEntity)];
            float penetration = 0.5f;
            float resistance = glm::clamp(penetration / 10.0f, 0.0f, 1.0f);

            // reduce velocity
            playerVelocity *= 0.95;

            // optional: accumulate “dig damage” to block
            float damage = 250.0f * dt;
            g.health -= damage;
            g.dirty = true;
        }
    }

    void updateMovement(float dt, bool pressing)
    {
        uint32_t headEntityId = entityIndex(player.entities.front());
        auto headIndexT = engine.ecs.entityToTransform[headEntityId];
        Transform &headT = engine.ecs.transforms[headIndexT];
        Mesh &headM = engine.ecs.meshes[engine.ecs.entityToMesh[headEntityId]];

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
        glm::vec2 targetPos = glm::vec2{headT.position};
        headT.position += playerVelocity * dt;
        tryMove(headT, headM, targetPos, dt);

        // All non head segments gets to make a move
        for (size_t i = 1; i < player.entities.size(); i++)
        {
            auto tIndex1 = engine.ecs.entityToTransform[player.entities[i - 1].id];
            auto tIndex2 = engine.ecs.entityToTransform[player.entities[i].id];
            Transform &t1 = engine.ecs.transforms[tIndex1];
            Transform &t2 = engine.ecs.transforms[tIndex2];
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
        headT.position += playerVelocity * dt;
        headT.commit();
    }

    void rotateHeadLeft(float dt)
    {
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

    void updateFPSCounter(float dt)
    {
        ZoneScoped; // PROFILER
        frameCount++;

        if (frameCount >= 400)
        {
            fps = round(frameCount / (fpsTimeSum / 1000.0f));
            frameCount = 0;
            fpsTimeSum = 0.0;
        }
        else
        {
            fpsTimeSum += dt;
        }
    }
};
