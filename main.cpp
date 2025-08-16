#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstdio>
#include "game.h"

Game::GameState gameState;

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect;
        GetClientRect(hwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        // --- Back buffer setup ---
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // --- Let the game render into memDC ---
        Game::renderGame(hwnd, memDC, gameState);

        // --- Blit buffer to screen in one go ---
        BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

        // --- Cleanup ---
        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1; // nonzero = handled, skip default erase
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// WinMain entry
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // 🔥 Attach a console so std::cout works
    AllocConsole();
    FILE *fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "Console attached! Starting program...\n";

    const wchar_t CLASS_NAME[] = L"RawWin32Window";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Desired window style (non-resizable)
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

    // Let Windows expand the size to account for borders & titlebar
    RECT rect = {0, 0, gameState.width, gameState.height};
    AdjustWindowRect(&rect, style, FALSE);

    int winWidth = rect.right - rect.left;
    int winHeight = rect.bottom - rect.top;
    int x = winHeight / 2;
    int y = winHeight / 2;

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Strongest Snake",
        style | WS_VISIBLE,
        x, y, winWidth, winHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
    {
        std::cerr << "Window creation failed!\n";
        return 1;
    }

    std::cout << "Window created successfully!\n";

    // Message loop
    MSG msg = {};
    bool running = true;
    double targetDelta = 1000.0 / 60.0;
    ULONGLONG lastTick = GetTickCount64();

    // Game init
    gameState.snakeCoords.push_back({10, 10});

    while (running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Timing
        ULONGLONG now = GetTickCount64();
        double delta = (double)(now - lastTick); // ms since last loop

        if (delta >= targetDelta)
        {
            lastTick = now;
            Game::tick(hwnd, gameState, delta);
        }
    }

    std::cout << "Goodbye from Strongest Snake!\n";
    return 0;
}
