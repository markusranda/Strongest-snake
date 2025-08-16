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
        //  1,  0  right
        // -1,  0  left
        //  0,  1  down
        //  0, -1  up
        int snakeDir[2] = {0, -1};
        double moveTimer = 500;
        int width = 800;
        int height = 800;
        int wCellCount = 21;
        int hCellCount = 21;
        std::vector<Coord> snakeCoords;
    };

    void tick(HWND hwnd, GameState &gameState, double dt);
    void renderGame(HWND hwnd, HDC hdc, const GameState &state);
}