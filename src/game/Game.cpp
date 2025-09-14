#include "Game.h"
#include <glm/glm.hpp>

Game::Game(Window &w)
    : window(w),
      boardBuffer{
          {glm::vec2(0.0f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f)}, // top center
          {glm::vec2(-0.5f, 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)}, // bottom left
          {glm::vec2(0.5f, 0.5f), glm::vec3(0.0f, 1.0f, 0.0f)},  // bottom right
      },
      engine(w.width, w.height, w, boardBuffer)
{
}
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