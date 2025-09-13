#include "Game.h"

Game::Game(Window &window, Engine &engine) : window(window), engine(engine) {}

void Game::run()
{
    engine.initVulkan();

    while (!window.shouldClose())
    {
        window.pollEvents();
        engine.drawFrame(lastFrameTime);

        double currentTime = glfwGetTime();
        lastFrameTime = (currentTime - lastTime) * 1000.0;
        lastTime = currentTime;
    }

    engine.awaitDeviceIdle();
    engine.cleanup();
}