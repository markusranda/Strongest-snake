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
    state.wCellCount = 20;
    state.hCellCount = 20;

    engine.initVulkan();
    state.snake.reset(glm::vec2{state.wCellCount / 2, state.hCellCount / 2});

    state.foodCoords[0] = rand() % state.wCellCount;
    state.foodCoords[1] = rand() % state.hCellCount;
}

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
    state.snake.act(delta);
    checkCollision();
}

void Game::checkCollision()
{
    auto head = state.snake.cells.front();

    // food
    if (head.x == state.foodCoords[0] && head.y == state.foodCoords[1])
    {
        state.snake.grow();
        state.foodCoords[0] = rand() % state.wCellCount;
        state.foodCoords[1] = rand() % state.hCellCount;
    }
    // walls
    else if (head.x < 0 || head.y < 0 || head.x >= state.wCellCount || head.y >= state.hCellCount)
    {
        Logger::debug("Crashed with wall");
        state.gameOver = true;
    }
    // self
    else
    {
        for (size_t i = 1; i < state.snake.cells.size(); i++)
        {
            if (state.snake.cells[i].x == head.x && state.snake.cells[i].y == head.y)
            {
                Logger::debug("Crashed with himself");
                state.gameOver = true;
                return;
            }
        }
    }
}

void Game::updateDirection()
{
    GLFWwindow *handle = window.getHandle();

    if (glfwGetKey(handle, GLFW_KEY_LEFT) == GLFW_PRESS && state.snake.dir[0] != 1)
    {
        state.snake.dir[0] = -1;
        state.snake.dir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_UP) == GLFW_PRESS && state.snake.dir[1] != 1)
    {
        state.snake.dir[0] = 0;
        state.snake.dir[1] = -1;
    }
    else if (glfwGetKey(handle, GLFW_KEY_RIGHT) == GLFW_PRESS && state.snake.dir[0] != -1)
    {
        state.snake.dir[0] = 1;
        state.snake.dir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_DOWN) == GLFW_PRESS && state.snake.dir[1] != -1)
    {
        state.snake.dir[0] = 0;
        state.snake.dir[1] = 1;
    }
}

// ----------------- rendering -----------------

std::vector<Quad> Game::renderGame()
{
    std::vector<Quad> quads;

    float cellW = static_cast<float>(window.width) / state.wCellCount;
    float cellH = static_cast<float>(window.height) / state.hCellCount;

    // Background
    quads.push_back(Quad{0, 0, (float)window.width, (float)window.height,
                         Colors::fromHex(Colors::GROUND_BEIGE, 1.0f),
                         window.width, window.height, ShaderType::FlatColor});

    // Snake
    std::vector<glm::vec2> snakePositions;
    state.snake.getInterpolatedCells(snakePositions);
    for (auto &pos : snakePositions)
    {
        quads.push_back(Quad{
            pos.x * cellW, pos.y * cellH,
            cellW, cellH,
            Colors::fromHex(Colors::LIME_GREEN, 1.0f),
            window.width, window.height, ShaderType::SnakeSkin});
    }

    // Food
    quads.push_back(Quad{
        state.foodCoords[0] * cellW, state.foodCoords[1] * cellH,
        cellW, cellH,
        Colors::fromHex(Colors::STRAWBERRY_RED, 1.0f),
        window.width, window.height, ShaderType::FlatColor});

    return quads;
}
