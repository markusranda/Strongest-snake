#pragma once
#include <vector>

namespace Game
{
    struct Coord
    {
        int x;
        int y;
    };

    struct GameState
    {
        int score = 0;
        double timeElapsed = 0;
        int snakeLength = 1;
        int snakeDir[2] = {0, 0};
        double moveTimer = 500;
        int wCellCount = 21;
        int hCellCount = 21;
        std::vector<Coord> snakeCoords;
    };

    void tick(HWND hwnd, GameState &gameState, double dt);
    void renderGame(HWND hwnd, HDC hdc, const GameState &state);
}