#include "Game.h"
#include <glm/glm.hpp>
#include "../engine/Quad.h"
#include "../tusj/Colors.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <deque>
#include "Collision.h"
#include <filesystem>

Game::Game(Window &w)
    : window(w),
      engine(w.width, w.height, w)
{
    try
    {
        Logger::info(std::filesystem::current_path().string());
        engine.initVulkan();
        state.cellSize = window.width / state.rows;

        state.background = Background{entityManager.createEntity()};

        auto playerEntity = entityManager.createEntity();
        auto transform = Transform{glm::vec2{std::floor(state.columns / 2) * state.cellSize, 1 * state.cellSize}, glm::vec2{state.cellSize, state.cellSize}};
        state.transforms[playerEntity] = transform;
        state.player = Player{playerEntity};

        state.camera = Camera{window.width, window.height};

        for (uint32_t y = 2; y < state.columns; y++)
        {
            for (uint32_t x = 0; x < state.rows; x++)
            {
                auto entity = entityManager.createEntity();
                auto transform = Transform{glm::vec2{x * state.cellSize, y * state.cellSize}, glm::vec2{state.cellSize, state.cellSize}};
                auto quad = Quad{transform.position.x, transform.position.y, transform.size.x, transform.size.y,
                                 Colors::fromHex(Colors::GROUND_BEIGE, 1.0f), ShaderType::Border, RenderLayer::World, "ground"};
                state.grounds.insert_or_assign(entity, Ground{entity, false});
                state.transforms.insert_or_assign(entity, transform);
                state.quads.insert_or_assign(entity, quad);
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

void Game::run()
{
    try
    {
        Logger::info("Starting game loop");
        while (!window.shouldClose())
        {
            window.pollEvents();

            double currentTime = glfwGetTime() * 1000.0; // ms
            double delta = currentTime - lastTime;
            lastTime = currentTime;

            if (state.gameOver)
                throw std::runtime_error("You fucked up");

            updateGame(delta);
            updateCamera();
            updateLifecycle();
            updateGraphics();

            engine.drawQuads(state.quads, state.camera);
        }
    }
    catch (std::exception e)
    {
        Logger::err(e.what());
    }
    catch (...)
    {
        Logger::err("Unknown exception thrown!");
    }
}

// ----------------- game logic -----------------

void Game::updateLifecycle()
{
    for (auto it = state.grounds.begin(); it != state.grounds.end();)
    {
        auto entity = it->second.entity;
        if (it->second.dead)
        {
            state.quads.erase(entity);
            state.transforms.erase(entity);
            it = state.grounds.erase(it);
            entityManager.destroyEntity(entity);
        }
        else
        {
            ++it;
        }
    }
}

void Game::updateCamera()
{
    Transform playerTransform = state.transforms[state.player.entity];
    state.camera.position = playerTransform.position - glm::vec2((float)window.width / 2.0f, (float)window.height / 2.0f);
}

void Game::updateGame(double delta)
{
    updateDirection();

    auto *playerTransform = &state.transforms[state.player.entity];
    if (playerTransform->dir.x != 0 || playerTransform->dir.y != 0)
    {
        state.holdTimer += delta;
    }
    else
    {
        state.holdTimer = 0;
    }

    if (state.holdTimer > 500)
    {
        playerTransform->position += playerTransform->dir * static_cast<float>(state.cellSize);
        state.holdTimer = 0;
    }

    checkCollision();
}

void Game::checkCollision()
{
    auto playerTransform = state.transforms[state.player.entity];
    for (auto &[entity, ground] : state.grounds)
    {
        auto groundTransform = state.transforms[entity];
        if (rectIntersects(playerTransform.position, playerTransform.size, groundTransform.position, groundTransform.size))
        {
            ground.dead = true;
        }
    }
}

void Game::updateDirection()
{
    GLFWwindow *handle = window.getHandle();
    Transform *playerTransform = &state.transforms[state.player.entity];
    if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS && playerTransform->dir.x != 1)
    {
        playerTransform->dir.x = -1.0f;
        playerTransform->dir.y = 0.0f;
    }
    else if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS && playerTransform->dir.y != 1)
    {
        playerTransform->dir.x = 0.0f;
        playerTransform->dir.y = -1.0f;
    }
    else if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS && playerTransform->dir.x != -1)
    {
        playerTransform->dir.x = 1.0f;
        playerTransform->dir.y = 0.0f;
    }
    else if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS && playerTransform->dir.y != -1)
    {
        playerTransform->dir.x = 0.0f;
        playerTransform->dir.y = 1.0f;
    }
    else
    {
        playerTransform->dir = glm::vec2(0.0f, 0.0f);
    }
}

// ----------------- rendering -----------------

void Game::updateGraphics()
{

    // Background
    state.quads.insert_or_assign(state.background.entity, Quad{state.camera.position.x, state.camera.position.y, (float)window.width, (float)window.height,
                                                               Colors::fromHex(Colors::SKY_BLUE, 1.0f),
                                                               ShaderType::FlatColor, RenderLayer::Background});

    // Update player position
    auto playerTransform = state.transforms[state.player.entity];
    state.quads.insert_or_assign(state.player.entity, Quad{playerTransform.position.x, playerTransform.position.y, playerTransform.size.x, playerTransform.size.y,
                                                           Colors::fromHex(Colors::MANGO_ORANGE, 1.0f),
                                                           ShaderType::FlatColor, RenderLayer::World});
}
