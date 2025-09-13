#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"

class Game
{
public:
    Game(Window &window, Engine &engine);

    void run();

private:
    Window window;
    Engine engine;

    float lastFrameTime = 0.0f;
    double lastTime = 0.0f;
};