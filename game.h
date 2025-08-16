#pragma once
#include <deque>

namespace Game
{
    struct Coord
    {
        int x;
        int y;
    };

    const double MOVE_TIMER_TIMEOUT = 125;

    struct GameState
    {
        double moveTimer = MOVE_TIMER_TIMEOUT;
        int width = 800;
        int height = 800;
        int wCellCount = 66;
        int hCellCount = 66;
        std::deque<Coord> snakeCoords;
        int snakeDir[2] = {0, -1};
        int foodCoords[2] = {0, 0};
        bool gameOver = false;
    };

    void tick(HWND hwnd, GameState &gameState, double dt);
    void renderGame(HWND hwnd, HDC hdc, const GameState &state);
}