#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Collision.h"
#include "../tusj/Colors.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <unordered_map>
#include <deque>
#include <filesystem>
#include <cstdlib>

struct Transform
{
    glm::vec2 position;
    glm::vec2 size;
    glm::vec2 dir = glm::vec2(0.0f);
    float rotation = 0.0f;
};

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

    // ECS-style state
    bool gameOver = false;
    EntityManager entityManager;
    Background background;
    Player player;
    Camera camera{100, 100};
    std::unordered_map<Entity, Transform> transforms;
    std::unordered_map<Entity, Quad> quads;
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
            background = {entityManager.createEntity()};

            // Player
            auto playerEntity = entityManager.createEntity();
            Transform transform{
                glm::vec2{std::floor(columns / 2) * cellSize, 1 * cellSize},
                glm::vec2{cellSize, cellSize}};
            transforms[playerEntity] = transform;
            player = {playerEntity};

            camera = Camera{window.width, window.height};

            // Ground tiles
            for (uint32_t y = 2; y < columns; y++)
            {
                for (uint32_t x = 0; x < rows; x++)
                {
                    auto entity = entityManager.createEntity();
                    Transform t{{x * cellSize, y * cellSize}, {cellSize, cellSize}};
                    Quad q{t.position.x, t.position.y, t.size.x, t.size.y,
                           Colors::fromHex(Colors::GROUND_BEIGE, 1.0f),
                           ShaderType::Border, RenderLayer::World, "ground"};
                    grounds.insert_or_assign(entity, Ground{entity, false});
                    transforms.insert_or_assign(entity, t);
                    quads.insert_or_assign(entity, q);
                }
            }
        }
        catch (const std::exception &e)
        {
            Logger::err(std::string("Failed to start game: ") + e.what());
        }
        catch (...)
        {
            Logger::err("Unknown exception thrown!");
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

            engine.drawQuads(quads, camera);
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
                quads.erase(entity);
                transforms.erase(entity);
                it = grounds.erase(it);
                entityManager.destroyEntity(entity);
            }
            else
            {
                ++it;
            }
        }
    }

    void updateCamera()
    {
        Transform playerTransform = transforms[player.entity];
        camera.position = playerTransform.position - glm::vec2(window.width / 2.0f, window.height / 2.0f);
    }

    void updateGame(double delta)
    {
        updateDirection();

        auto &playerTransform = transforms[player.entity];
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
        auto playerTransform = transforms[player.entity];
        for (auto &[entity, ground] : grounds)
        {
            auto groundTransform = transforms[entity];
            if (rectIntersects(playerTransform.position, playerTransform.size, groundTransform.position, groundTransform.size))
            {
                ground.dead = true;
            }
        }
    }

    void updateDirection()
    {
        GLFWwindow *handle = window.getHandle();
        auto &playerTransform = transforms[player.entity];

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
        quads.insert_or_assign(
            background.entity,
            Quad{camera.position.x, camera.position.y,
                 (float)window.width, (float)window.height,
                 Colors::fromHex(Colors::SKY_BLUE, 1.0f),
                 ShaderType::FlatColor, RenderLayer::Background});

        auto playerTransform = transforms[player.entity];
        quads.insert_or_assign(
            player.entity,
            Quad{playerTransform.position.x, playerTransform.position.y,
                 playerTransform.size.x, playerTransform.size.y,
                 Colors::fromHex(Colors::MANGO_ORANGE, 1.0f),
                 ShaderType::FlatColor, RenderLayer::World});
    }
};
