#include <iostream>
#include <windows.h>
#include "game.h"

namespace Game
{

    void updateGame(GameState &state, double delta);
    void moveSnake(GameState &state);
    void updateTimers(GameState &state, double delta);
    void updateDirection(GameState &state);

    void tick(HWND hwnd, GameState &state, double delta)
    {
        updateGame(state, delta);

        // This forces a redraw
        InvalidateRect(hwnd, nullptr, FALSE);
    }

    void updateGame(GameState &state, double delta)
    {
        updateTimers(state, delta);
        updateDirection(state);

        if (state.moveTimer <= 0)
        {
            state.moveTimer = MOVE_TIMER_TIMEOUT;
            moveSnake(state);
        }
    }

    void updateDirection(GameState &state)
    {
        SHORT left = GetAsyncKeyState(VK_LEFT);
        SHORT up = GetAsyncKeyState(VK_UP);
        SHORT right = GetAsyncKeyState(VK_RIGHT);
        SHORT down = GetAsyncKeyState(VK_DOWN);
        if (left & 0x8000)
        {
            state.snakeDir[0] = -1;
            state.snakeDir[1] = 0;
        }
        else if (up & 0x8000)
        {
            state.snakeDir[0] = 0;
            state.snakeDir[1] = -1;
        }
        else if (right & 0x8000)
        {
            state.snakeDir[0] = 1;
            state.snakeDir[1] = 0;
        }
        else if (down & 0x8000)
        {
            state.snakeDir[0] = 0;
            state.snakeDir[1] = 1;
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
        HBRUSH foodBrush = CreateSolidBrush(RGB(139, 5, 5));
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

            RECT sRect;
            sRect.top = y;
            sRect.left = x;
            sRect.bottom = y + cellSize;
            sRect.right = x + cellSize;
            FillRect(hdc, &sRect, snakeBrush);
        }

        // Draw food
        RECT fRect;
        int fx = state.foodCoords[0] * cellSize;
        int fy = state.foodCoords[1] * cellSize;
        fRect.top = fy;
        fRect.left = fx;
        fRect.bottom = fy + cellSize;
        fRect.right = fx + cellSize;
        FillRect(hdc, &fRect, foodBrush);

        // restore and clean up
        SelectObject(hdc, oldPen);
        DeleteObject(snakeBrush);
        DeleteObject(foodBrush);
        DeleteObject(brush);
        DeleteObject(pen);
    }
}
