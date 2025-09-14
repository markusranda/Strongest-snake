#include "Game.h"
#include <glm/glm.hpp>
#include "../engine/Quad.h"
#include "../tusj/Colors.h"
#include <GLFW/glfw3.h>
#include <cstdlib>

static constexpr double MOVE_TIMER_TIMEOUT = 200.0; // ms per move

Game::Game(Window &w)
    : window(w),
      engine(w.width, w.height, w)
{
    // init game state
    state.wCellCount = 20;
    state.hCellCount = 20;
    state.snakeCoords.push_back({state.wCellCount / 2, state.hCellCount / 2});

    // compute cell size
    float cellW = static_cast<float>(window.width) / state.wCellCount;
    float cellH = static_cast<float>(window.height) / state.hCellCount;
    background.push_back(Quad{
        0, 0,
        (float)window.width, (float)window.height,
        Colors::fromHex(Colors::WHITE, 1.0f),
        window.width, window.height});

    for (int x = 0; x < state.wCellCount; x++)
    {
        for (int y = 0; y < state.hCellCount; y++)
        {
            background.push_back(Quad{
                x * cellW, y * cellH,
                cellW - 1, cellH - 1,
                Colors::fromHex(Colors::GROUND_BEIGE, 0.75f),
                window.width, window.height});
        }
    }

    Logger::info(std::to_string(background.size()));

    // place initial food
    state.foodCoords[0] = rand() % state.wCellCount;
    state.foodCoords[1] = rand() % state.hCellCount;
}

void Game::run()
{
    engine.initVulkan();

    while (!window.shouldClose())
    {
        window.pollEvents();

        double currentTime = glfwGetTime() * 1000.0; // ms
        double delta = currentTime - lastTime;
        lastTime = currentTime;

        if (state.gameOver)
        {
            throw std::runtime_error("You fucked up");
        }

        updateGame(delta);

        auto quads = renderGame();
        engine.drawQuads(quads);
    }

    engine.awaitDeviceIdle();
    engine.cleanup();
}

// ----------------- game logic -----------------

void Game::updateGame(double delta)
{
    updateDirection();
    state.moveTimer -= delta;

    if (state.moveTimer <= 0)
    {
        state.moveTimer = MOVE_TIMER_TIMEOUT;
        Coord oldTail = moveSnake();
        checkCollision(oldTail);
    }
}

Coord Game::moveSnake()
{
    Coord tailCopy = state.snakeCoords.back();
    Coord newHead = {state.snakeCoords.front().x + state.snakeDir[0],
                     state.snakeCoords.front().y + state.snakeDir[1]};

    state.snakeCoords.push_front(newHead);
    state.snakeCoords.pop_back();

    return tailCopy;
}

void Game::checkCollision(Coord oldTail)
{
    const Coord head = state.snakeCoords.front();

    // food
    if (head.x == state.foodCoords[0] && head.y == state.foodCoords[1])
    {
        state.snakeCoords.push_back(oldTail); // grow
        state.foodCoords[0] = rand() % state.wCellCount;
        state.foodCoords[1] = rand() % state.hCellCount;
    }
    // walls
    else if (head.x < 0 || head.y < 0 || head.x >= state.wCellCount || head.y >= state.hCellCount)
    {
        state.gameOver = true;
    }
    // self collision
    else
    {
        for (size_t i = 1; i < state.snakeCoords.size(); i++)
        {
            if (state.snakeCoords[i].x == head.x && state.snakeCoords[i].y == head.y)
            {
                state.gameOver = true;
                return;
            }
        }
    }
}

void Game::updateDirection()
{
    GLFWwindow *handle = window.getHandle();

    if (glfwGetKey(handle, GLFW_KEY_LEFT) == GLFW_PRESS && state.snakeDir[0] != 1)
    {
        state.snakeDir[0] = -1;
        state.snakeDir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_UP) == GLFW_PRESS && state.snakeDir[1] != 1)
    {
        state.snakeDir[0] = 0;
        state.snakeDir[1] = -1;
    }
    else if (glfwGetKey(handle, GLFW_KEY_RIGHT) == GLFW_PRESS && state.snakeDir[0] != -1)
    {
        state.snakeDir[0] = 1;
        state.snakeDir[1] = 0;
    }
    else if (glfwGetKey(handle, GLFW_KEY_DOWN) == GLFW_PRESS && state.snakeDir[1] != -1)
    {
        state.snakeDir[0] = 0;
        state.snakeDir[1] = 1;
    }
}

// ----------------- rendering -----------------

std::vector<Quad> Game::renderGame()
{
    // Start with a copy of background
    std::vector<Quad> quads = background;

    float cellW = static_cast<float>(window.width) / state.wCellCount;
    float cellH = static_cast<float>(window.height) / state.hCellCount;

    // snake
    for (const auto &coord : state.snakeCoords)
    {
        quads.push_back(Quad{
            coord.x * cellW, coord.y * cellH,
            cellW, cellH,
            Colors::fromHex(Colors::LIME_GREEN, 1.0f),
            window.width, window.height});
    }

    // food
    quads.push_back(Quad{
        state.foodCoords[0] * cellW, state.foodCoords[1] * cellH,
        cellW, cellH,
        Colors::fromHex(Colors::STRAWBERRY_RED, 1.0f),
        window.width, window.height});

    return quads;
}
