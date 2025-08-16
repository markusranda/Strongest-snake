#include <iostream>
#include <windows.h>
#include "game.h"

namespace Game
{

    void updateGame(GameState &state, double delta);
    void moveSnake(GameState &state);
    void updateTimers(GameState &state, double delta);

    void tick(HWND hwnd, GameState &state, double delta)
    {
        updateGame(state, delta);

        // This forces a redraw
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void updateGame(GameState &state, double delta)
    {
        updateTimers(state, delta);

        if (state.moveTimer <= 0)
        {
            state.moveTimer = 500;
            moveSnake(state);
        }
    }

    void updateTimers(GameState &state, double delta)
    {
        state.moveTimer -= delta;
    }

    void moveSnake(GameState &state)
    {
        for (Coord &coord : state.snakeCoords)
        {
            coord.x += state.snakeDir[0];
            coord.y += state.snakeDir[1];
        }
    }

    void renderGame(HWND hwnd, HDC hdc, const GameState &state)
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        const int cellSize = width / state.wCellCount;

        // Draw background
        HBRUSH brush = CreateSolidBrush(RGB(139, 68, 20));    // dark blue background
        HBRUSH snakeBrush = CreateSolidBrush(RGB(0, 200, 0)); // dark blue background
        FillRect(hdc, &rect, brush);

        // Pen
        int penSize = 1;
        HPEN pen = CreatePen(PS_SOLID, penSize, RGB(208, 147, 107));
        HPEN oldPen = (HPEN)SelectObject(hdc, pen);

        // Draw col lines (vertical)
        for (int x = 0; x <= width; x += cellSize)
        {
            MoveToEx(hdc, x, 0, nullptr);
            LineTo(hdc, x, height);
        }

        // Draw row lines (horizontal)
        for (int y = 0; y <= height; y += cellSize)
        {
            MoveToEx(hdc, 0, y, nullptr);
            LineTo(hdc, width, y);
        }

        // Draw snake
        for (const Coord &coord : state.snakeCoords)
        {
            int x = coord.x * cellSize;
            int y = coord.y * cellSize;

            RECT rect;
            rect.top = y;
            rect.left = x;
            rect.bottom = y + cellSize;
            rect.right = x + cellSize;
            FillRect(hdc, &rect, snakeBrush);
        }

        // restore and clean up
        SelectObject(hdc, oldPen);
        DeleteObject(snakeBrush);
        DeleteObject(brush);
        DeleteObject(pen);
    }
}
