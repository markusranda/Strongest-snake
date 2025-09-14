#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"
#include <deque>
#include "Snake.H"

struct Coord
{
    int x, y;
};

struct SnakeSegment
{
    Coord last;
    Coord next;
};

static constexpr double MOVE_TIMER_TIMEOUT = 200.0; // ms per cell

struct GameState
{
    int wCellCount;
    int hCellCount;

    bool gameOver = false;

    int foodCoords[2];

    Snake snake;
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