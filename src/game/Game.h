#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"

class Game
{
public:
    Game(Window &window);

    void run();

private:
    Window window;
    Engine engine;

    float lastFrameTime = 0.0f;
    double lastTime = 0.0f;

    std::vector<Quad> Game::getBackground();
};