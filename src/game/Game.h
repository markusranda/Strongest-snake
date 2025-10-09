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
            engine.initVulkan();

            // --- Background ---
            background = {engine.ecs.createEntity()};
            auto backgroundTranslate = Transform{glm::vec2{0, 0}, glm::vec2{window.width, window.height}};
            engine.quads.insert_or_assign(
                background.entity,
                Quad{ShaderType::FlatColor, RenderLayer::Background, Colors::fromHex(Colors::SKY_BLUE, 1.0f), "background"});
            engine.transforms.insert_or_assign(background.entity, backgroundTranslate);

            // ---- Player ----
            for (size_t i = 0; i < 4; i++)
            {
                auto entity = engine.ecs.createEntity();
                Transform transform = Transform{
                    glm::vec2{std::floor(columns / 2) * snakeSize - (i * snakeSize), snakeSize},
                    glm::vec2{snakeSize, snakeSize},
                    "player"};
                engine.transforms.insert_or_assign(entity, transform);
                player.entities[i] = entity;
                engine.quads.insert_or_assign(
                    entity,
                    Quad{ShaderType::DirArrow, RenderLayer::World, Colors::fromHex(Colors::MANGO_ORANGE, 1.0f), "player"});
            }

            // --- Camera ---
            camera = Camera{window.width, window.height};

            // --- Ground ----
            for (uint32_t y = 2; y < rows; y++)
            {
                for (uint32_t x = 0; x < columns; x++)
                {
                    auto entity = engine.ecs.createEntity();
                    Transform t{{x * groundSize, y * groundSize}, {groundSize, groundSize}, "ground"};
                    Quad q{ShaderType::Border, RenderLayer::World, Colors::fromHex(Colors::GROUND_BEIGE, 1.0f), "ground"};
                    grounds.insert_or_assign(entity, Ground{entity, false});
                    engine.transforms.insert_or_assign(entity, t);
                    engine.quads.insert_or_assign(entity, q);
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

        while (!window.shouldClose())
        {
            window.pollEvents();

            double currentTime = glfwGetTime() * 1000.0;
            double delta = currentTime - lastTime;
            lastTime = currentTime;

            if (gameOver)
                throw std::runtime_error("You fucked up");

            updateGame(delta);
            updateCamera();
            updateLifecycle();

            engine.draw(camera);

            updateFPSCounter(delta);
        }
    }

    // --- Game logic ---
    void updateLifecycle()
    {
        for (auto it = grounds.begin(); it != grounds.end();)
        {
            auto entity = it->second.entity;
            if (it->second.dead)
            {
                engine.quads.erase(entity);
                engine.transforms.erase(entity);
                it = grounds.erase(it);
                engine.ecs.destroyEntity(entity);
            }
            else
            {
                ++it;
            }
        }
    }

    void updateCamera()
    {
        Transform playerTransform = engine.transforms.at(player.entities.front());
        camera.position = playerTransform.position - glm::vec2(window.width / 2.0f, window.height / 2.0f);

        engine.transforms.at(background.entity).position.x = camera.position.x;
        engine.transforms.at(background.entity).position.y = camera.position.y;
    }

    void updateGame(double delta)
    {

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
        auto playerTransform = engine.transforms.at(player.entities.front());
        for (auto &[entity, ground] : grounds)
        {
            auto groundTransform = engine.transforms.at(entity);
            if (rectIntersects(playerTransform.position, playerTransform.size, groundTransform.position, groundTransform.size))
            {
                ground.dead = true;
            }
        }
    }

    void drawDebugBox(glm::vec2 point)
    {
        auto randomEntiy = engine.ecs.createEntity();
        Transform randomTrans = Transform{
            point - (float)(8 / 2),
            glm::vec2{8, 8},
            "player"};
        engine.transforms.insert_or_assign(randomEntiy, randomTrans);
        engine.quads.insert_or_assign(
            randomEntiy,
            Quad{ShaderType::Border, RenderLayer::World, Colors::fromHex(Colors::BLACK, 1.0f), "player"});
    }

    void moveForward(float dt)
    {
        auto &playerTransform = engine.transforms.at(player.entities.front());
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
            glm::vec2 prevPos = engine.transforms.at(player.entities[i - 1]).position;
            glm::vec2 &pos = engine.transforms.at(player.entities[i]).position;

            glm::vec2 dir = prevPos - pos;
            float dist = glm::length(dir);
            if (dist > snakeSize)
            {
                dir = glm::normalize(dir);
                pos = prevPos - dir * (float)snakeSize;
            }

            // Optionally adjust rotation
            engine.transforms.at(player.entities[i]).rotation = atan2(dir.y, dir.x);
        }

        // Move head
        engine.transforms.at(player.entities[0]).position += playerVelocity * dt;
    }

    void rotateHeadLeft(float dt)
    {
        Transform transform = engine.transforms.at(player.entities[2]);
        glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);
        glm::vec2 leftDir = glm::vec2(forward.y, -forward.x);
        glm::vec2 radiusCenter = transform.position + leftDir * rotationRadius;

        float rotationSpeed = 5.0f;         // tweak this
        float maxDistance = rotationRadius; // Increase to stop earlier

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i];
            Transform &transform = engine.transforms.at(entity);
            glm::vec2 localCenter = transform.position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(localCenter.y, -localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(transform.position - radiusCenter);
            if (dist < maxDistance)
                continue;

            transform.position = transform.position - dt * (localCenter);
            transform.rotation += deltaAngle * dt * rotationSpeed;
            break;
        }
    }

    void rotateHeadRight(float dt)
    {
        Transform transform = engine.transforms.at(player.entities[2]);
        glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);
        glm::vec2 rightDir = glm::vec2(-forward.y, forward.x);
        glm::vec2 radiusCenter = transform.position + rightDir * rotationRadius;

        for (size_t i = 0; i < 2; i++)
        {
            // Move segment
            Entity entity = player.entities[i];
            Transform &transform = engine.transforms.at(entity);
            glm::vec2 localCenter = transform.position - radiusCenter;
            glm::vec2 forward = SnakeMath::getRotationVector2(transform.rotation);

            // Rotate segment
            glm::vec2 tangent = glm::normalize(glm::vec2(-localCenter.y, localCenter.x));
            float currentAngle = atan2(forward.y, forward.x);
            float targetAngle = atan2(tangent.y, tangent.x);
            float deltaAngle = targetAngle - currentAngle;
            if (deltaAngle > SnakeMath::PI)
                deltaAngle -= 2.0f * SnakeMath::PI;
            if (deltaAngle < -SnakeMath::PI)
                deltaAngle += 2.0f * SnakeMath::PI;

            float dist = glm::length(transform.position - radiusCenter);
            if (dist < maxDistance)
                continue;

            transform.position = transform.position - dt * (localCenter);
            transform.rotation += deltaAngle * dt * rotationSpeed;
            break;
        }
    }

    void updateFPSCounter(float dt)
    {
        frameCount++;

        if (frameCount >= 10)
        {
            fps = frameCount / (fpsTimeSum / 1000.0f);
            frameCount = 0;
            fpsTimeSum = 0.0;
        }
        else
        {
            fpsTimeSum += dt;
        }
    }
};
