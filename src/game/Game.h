#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"
#include <deque>

struct Ground
{
    glm::vec2 pos;
    glm::vec2 size;
    bool dead = false;
};

struct Player
{
    glm::vec2 pos;
    glm::vec2 size;
    glm::vec2 dir = glm::vec2(0.0f, 0.0f);
};

struct GameState
{
    bool gameOver = false;
    Player player;
    std::vector<Ground> groundList;

    uint32_t rows = 20;
    uint32_t columns = 20;
    uint32_t cellSize = 1;

    float holdTimer = 0;
};

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

    float cellW;
    float cellH;
    GameState state;
    std::vector<Quad> background;

    void updateGame(double delta);
    void Game::updateDirection();
    std::vector<Quad> Game::renderGame();
    void Game::checkCollision();
};