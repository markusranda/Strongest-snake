#pragma once
#include "../engine/Engine.h"
#include "../engine/Window.h"
#include "../engine/Vertex.h"
#include <deque>

struct Coord
{
    int x, y;
};

struct GameState
{
    int wCellCount;
    int hCellCount;
    double moveTimer = 0.0;
    bool gameOver = false;

    std::deque<Coord> snakeCoords;
    int snakeDir[2] = {1, 0}; // start moving right
    int foodCoords[2];
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
    void Game::checkCollision(Coord oldTail);
    Coord Game::moveSnake();
};