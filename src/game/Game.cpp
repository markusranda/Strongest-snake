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

    std::vector<Quad> background = {
        Quad({0.0f, 0.5f}, {0.5f, 0.5f}, {0.5f, 0.0f}, {0.0f, 0.0f}, {1, 0, 0})};

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
