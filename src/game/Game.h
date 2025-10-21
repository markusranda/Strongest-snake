#pragma once
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/Vertex.h"
#include "engine/EntityManager.h"
#include "engine/Camera.h"
#include "Collision.h"
#include "../tusj/Colors.h"
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <deque>
#include <filesystem>
#include <cstdlib>
#include "engine/Transform.h"
#include "SnakeMath.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>

// PROFILING
#include "tracy/Tracy.hpp"

struct Ground
{
    Entity entity;
    bool dead;
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
    const uint32_t rows = 200;
    const uint32_t columns = 20;
    const uint32_t groundSize = 64;

    // -- Player ---
    const float thrustPower = 900.0f; // pixels per second squared
    const float friction = 6.0f;      // friction coefficient
    const uint32_t snakeSize = 32;

    glm::vec2 playerVelocity = {0.0f, 0.0f};
    float rotationSpeed = 5.0f; // tweak this

    // Rotation
    float rotationRadius = 75.0f;       // Increase to stop earlier
    float maxDistance = rotationRadius; // Increase to stop earlier

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
            Logger::info(std::filesystem::current_path().string());
            engine.initVulkan("assets/fonts.png");

            // --- Background ---
            Transform backgroundTransform = Transform{glm::vec2{0, 0}, glm::vec2{window.width, window.height}};
            Quad backgroundQuad = Quad{ShaderType::FlatColor, RenderLayer::Background, Colors::fromHex(Colors::SKY_BLUE, 1.0f), "background"};
            background = {engine.ecs.createEntity(backgroundTransform, backgroundQuad)};

            // ---- Player ----
            for (size_t i = 0; i < 4; i++)
            {
                Transform transform = Transform{
                    glm::vec2{std::floor(columns / 2) * snakeSize - (i * snakeSize), snakeSize},
                    glm::vec2{snakeSize, snakeSize},
                    "player"};
                Quad quad = Quad{ShaderType::DirArrow, RenderLayer::World, Colors::fromHex(Colors::MANGO_ORANGE, 1.0f), "player"};
                auto entity = engine.ecs.createEntity(transform, quad);
                player.entities[i] = entity;
            }

            // --- Camera ---
            camera = Camera{window.width, window.height};

            // --- Ground ----
            for (uint32_t y = 2; y < rows; y++)
            {
                for (uint32_t x = 0; x < columns; x++)
                {
                    Transform t = Transform{{x * groundSize, y * groundSize}, {groundSize, groundSize}, "ground"};
                    Quad q = Quad{ShaderType::Border, RenderLayer::World, Colors::fromHex(Colors::GROUND_BEIGE, 1.0f), "ground"};
                    Entity entity = engine.ecs.createEntity(t, q);
                    grounds.insert_or_assign(entity, Ground{entity, false});
                }
            }
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
        Logger::info("Starting game loop");

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

            updateCamera();

            updateLifecycle();

            engine.draw(camera, fps);

            updateFPSCounter(delta);
        }
    }

    // --- Game logic ---
    void updateLifecycle()
    {
        ZoneScoped; // PROFILER
        for (auto &[entity, ground] : grounds)
        {
            if (ground.dead)
            {
                engine.ecs.destroyEntity(ground.entity);
            }
        }
    }

    void updateCamera()
    {
        ZoneScoped; // PROFILER
        auto playerIndexT = engine.ecs.entityToTransform[player.entities.front().id];
        auto backgroundIndexT = engine.ecs.entityToTransform[entityIndex(background.entity)];

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
        if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS)
            moveForward(dt);

        checkCollision();
    }

    void checkCollision()
    {
        auto playerIndexT = engine.ecs.entityToTransform[player.entities.front().id];
        Transform &playerTransform = engine.ecs.transforms[playerIndexT];

        std::vector<Entity> deadGrounds;
        for (auto &[entity, ground] : grounds)
        {
            auto gIndexT = engine.ecs.entityToTransform[entityIndex(entity)];

            // Cleanup dead entities
            if (gIndexT == UINT32_MAX)
            {
                deadGrounds.push_back(entity);
                continue;
            }

            Transform &groundTransform = engine.ecs.transforms[gIndexT];
            if (rectIntersects(playerTransform.position, playerTransform.size, groundTransform.position, groundTransform.size))
            {
                ground.dead = true;
            }
        }

        for (auto &entity : deadGrounds)
        {
            grounds.erase(entity);
        }
    }

    void moveForward(float dt)
    {
        auto playerIndexT = engine.ecs.entityToTransform[player.entities.front().id];
        Transform &playerTransform = engine.ecs.transforms[playerIndexT];
        glm::vec2 acceleration = {0.0f, 0.0f};
        glm::vec2 forward = SnakeMath::getRotationVector2(playerTransform.rotation);
        acceleration = forward * thrustPower;

        // --- Velocity integration ---
        playerVelocity += acceleration * dt;

        // --- Apply friction ---
        playerVelocity -= playerVelocity * friction * dt;

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
        auto headIndex = engine.ecs.entityToTransform[(player.entities[0]).id];
        Transform &head = engine.ecs.transforms[headIndex];
        head.position += playerVelocity * dt;
        head.commit();
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
