#pragma once
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/Vertex.h"
#include "engine/EntityManager.h"
#include "engine/Camera.h"
#include "Collision.h"
#include "../tusj/Colors.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <deque>
#include <filesystem>
#include <cstdlib>
#include "engine/Transform.h"

struct Ground
{
    Entity entity;
    bool dead;
};
struct Player
{
    Entity entity;
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

    bool gameOver = false;
    Background background;
    Player player;
    Camera camera{1, 1};
    std::unordered_map<Entity, Ground> grounds;

    // Game settings
    uint32_t rows = 20;
    uint32_t columns = 20;
    uint32_t cellSize = 1;
    float holdTimer = 0;

    // Timing
    double lastTime = 0.0;

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

            cellSize = window.width / rows;

            // --- Background ---
            background = {engine.ecs.createEntity()};
            auto backgroundTranslate = Transform{glm::vec2{0, 0}, glm::vec2{window.width, window.height}};
            engine.quads.insert_or_assign(
                background.entity,
                Quad{ShaderType::FlatColor, RenderLayer::Background, Colors::fromHex(Colors::SKY_BLUE, 1.0f), "background"});
            engine.transforms.insert_or_assign(background.entity, backgroundTranslate);

            // ---- Player ----
            auto playerEntity = engine.ecs.createEntity();
            Transform transform = Transform{
                glm::vec2{std::floor(columns / 2) * cellSize, 1 * cellSize},
                glm::vec2{cellSize, cellSize},
                3.14f / 4,
                "player"};
            engine.transforms.insert_or_assign(playerEntity, transform);
            player = {playerEntity};
            engine.quads.insert_or_assign(
                playerEntity,
                Quad{ShaderType::FlatColor, RenderLayer::World, Colors::fromHex(Colors::MANGO_ORANGE, 1.0f), "player"});

            camera = Camera{window.width, window.height};

            // --- Ground ----
            for (uint32_t y = 2; y < columns; y++)
            {
                for (uint32_t x = 0; x < rows; x++)
                {
                    auto entity = engine.ecs.createEntity();
                    Transform t{{x * cellSize, y * cellSize}, {cellSize, cellSize}, "ground"};
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
            updateGraphics();

            engine.draw(camera);
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
        Transform playerTransform = engine.transforms.at(player.entity);
        camera.position = playerTransform.position - glm::vec2(window.width / 2.0f, window.height / 2.0f);

        engine.transforms.at(background.entity).position.x = camera.position.x;
        engine.transforms.at(background.entity).position.y = camera.position.y;
    }

    void updateGame(double delta)
    {
        updateDirection();

        auto &playerTransform = engine.transforms.at(player.entity);
        if (playerTransform.dir.x != 0 || playerTransform.dir.y != 0)
            holdTimer += delta;
        else
            holdTimer = 0;

        if (holdTimer > 500)
        {
            playerTransform.position += playerTransform.dir * static_cast<float>(cellSize);
            holdTimer = 0;
        }

        checkCollision();
    }

    void checkCollision()
    {
        auto playerTransform = engine.transforms.at(player.entity);
        for (auto &[entity, ground] : grounds)
        {
            auto groundTransform = engine.transforms.at(entity);
            if (rectIntersects(playerTransform.position, playerTransform.size, groundTransform.position, groundTransform.size))
            {
                ground.dead = true;
            }
        }
    }

    void updateDirection()
    {
        GLFWwindow *handle = window.getHandle();
        auto &playerTransform = engine.transforms.at(player.entity);

        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS && playerTransform.dir.x != 1)
            playerTransform.dir = {-1.0f, 0.0f};
        else if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS && playerTransform.dir.y != 1)
            playerTransform.dir = {0.0f, -1.0f};
        else if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS && playerTransform.dir.x != -1)
            playerTransform.dir = {1.0f, 0.0f};
        else if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS && playerTransform.dir.y != -1)
            playerTransform.dir = {0.0f, 1.0f};
        else
            playerTransform.dir = {0.0f, 0.0f};
    }

    // --- Rendering ---
    void updateGraphics()
    {
    }
};
