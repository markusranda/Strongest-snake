#include "Game.h"
#include <glm/glm.hpp>
#include "../engine/Quad.h"

Game::Game(Window &w)
    : window(w),
      engine(w.width, w.height, w)
{
}

void Game::run()
{
    engine.initVulkan();

    std::vector<Quad> background = getBackground();

    while (!window.shouldClose())
    {
        window.pollEvents();
        engine.drawQuads(background);

        double currentTime = glfwGetTime();
        lastFrameTime = (currentTime - lastTime) * 1000.0;
        lastTime = currentTime;
    }

    engine.awaitDeviceIdle();
    engine.cleanup();
}

std::vector<Quad> Game::getBackground()
{
    std::vector<Quad> vertices;
    float len = 50;
    float colSize = window.width / len;
    float rowSize = window.height / len;
    const auto color = glm::vec3{1, 0, 0};

    for (uint32_t x = 0; x < len; x++)
    {
        for (uint32_t y = 0; y < len; y++)
        {
            Quad quad = {x * colSize, y * rowSize, colSize - 1.0f, rowSize - 1.0f, color, window.width, window.height};
            vertices.push_back(quad);
        }
    }

    return vertices;
}