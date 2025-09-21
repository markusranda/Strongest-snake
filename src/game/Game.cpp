#include "Game.h"
#include <glm/glm.hpp>
#include "../engine/Quad.h"
#include "../tusj/Colors.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <deque>

Game::Game(Window &w)
    : window(w),
      engine(w.width, w.height, w)
{
    engine.initVulkan();

    state.cellSize = window.width / state.rows;
    state.groundList.reserve((state.rows - 2) * state.columns);
    for (uint32_t y = 2; y < state.columns; y++)
    {
        for (uint32_t x = 0; x < state.rows; x++)
        {
            state.groundList.push_back(
                Ground{glm::vec2{x * state.cellSize, y * state.cellSize}, glm::vec2{state.cellSize, state.cellSize}});
        }
    }

    state.player.pos.x = std::floor(state.columns / 2) * state.cellSize;
    state.player.pos.y = 1 * state.cellSize;

    state.player.size.x = state.cellSize;
    state.player.size.y = state.cellSize;
}

// MAP
// [
//     00000000000000000000
//     00000000000000000000
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
//     xxxxxxxxxxxxxxxxxxxx
// ]

void Game::run()
{
    try
    {
        while (!window.shouldClose())
        {
            window.pollEvents();

            double currentTime = glfwGetTime() * 1000.0; // ms
            double delta = currentTime - lastTime;
            lastTime = currentTime;

            if (state.gameOver)
                throw std::runtime_error("You fucked up");

            updateGame(delta);
            auto quads = renderGame();
            engine.drawQuads(quads);
        }
    }
    catch (std::exception e)
    {
        Logger::err(e.what());
    }

    engine.awaitDeviceIdle();
    engine.cleanup();
}

// ----------------- game logic -----------------

void Game::updateGame(double delta)
{
    updateDirection();

    state.player.pos = state.player.pos + (state.player.dir * static_cast<float>(state.cellSize));
    state.player.dir = glm::vec2(0.0f, 0.0f);

    checkCollision();
}

void Game::checkCollision()
{
    for (auto &ground : state.groundList)
    {
        bool hitX = false;
        bool hitY = false;
        if (state.player.pos.x >= ground.pos.x && state.player.pos.x < ground.pos.x + ground.size.x)
        {
            hitX = true;
        }
        if (state.player.pos.y >= ground.pos.y && state.player.pos.y < ground.pos.y + ground.size.y)
        {
            hitY = true;
        }

        if (hitX && hitY)
        {
            ground.dead = true;
        }
    }
}

void Game::updateDirection()
{
    GLFWwindow *handle = window.getHandle();

    if (glfwGetKey(handle, GLFW_KEY_LEFT) == GLFW_PRESS && state.player.dir[0] != 1)
    {
        state.player.dir[0] = -1;
        state.player.dir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_UP) == GLFW_PRESS && state.player.dir[1] != 1)
    {
        state.player.dir[0] = 0;
        state.player.dir[1] = -1;
    }
    else if (glfwGetKey(handle, GLFW_KEY_RIGHT) == GLFW_PRESS && state.player.dir[0] != -1)
    {
        state.player.dir[0] = 1;
        state.player.dir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_DOWN) == GLFW_PRESS && state.player.dir[1] != -1)
    {
        state.player.dir[0] = 0;
        state.player.dir[1] = 1;
    }
}

// ----------------- rendering -----------------

std::vector<Quad> Game::renderGame()
{
    std::vector<Quad> quads;

    // Background
    quads.push_back(Quad{0, 0, (float)window.width, (float)window.height,
                         Colors::fromHex(Colors::SKY_BLUE, 1.0f),
                         window.width, window.height, ShaderType::FlatColor});

    for (auto &ground : state.groundList)
    {
        if (!ground.dead)
        {
            quads.push_back(Quad{ground.pos.x, ground.pos.y, ground.size.x, ground.size.y,
                                 Colors::fromHex(Colors::GROUND_BEIGE, 1.0f),
                                 window.width, window.height, ShaderType::Border});
        }
    }

    quads.push_back(Quad{state.player.pos.x, state.player.pos.y, state.player.size.x, state.player.size.y,
                         Colors::fromHex(Colors::MANGO_ORANGE, 1.0f),
                         window.width, window.height, ShaderType::FlatColor});

    return quads;
}
