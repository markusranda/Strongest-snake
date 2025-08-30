#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstdio>
#include "game.h"
// #include <vulkan/vulkan.h>
#include <vector>
// #include "renderer/vulkan_renderer.h"

// Definitions
Game::GameState createGame();

// State
Game::GameState gameState = createGame();
// std::unique_ptr<VulkanRenderer> gRenderer;

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        // if (gRenderer)
        // {
        // gRenderer->drawFrame();
        // }
        ValidateRect(hwnd, nullptr); // mark paint as handled
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
    try
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

        // --- Initialize Vulkan renderer here ---
        // gRenderer = std::make_unique<VulkanRenderer>(hwnd, gameState.width, gameState.height);
        // gRenderer->init();

        // Message loop
        MSG msg = {};
        bool running = true;
        double targetDelta = 1000.0 / 60.0;
        ULONGLONG lastTick = GetTickCount64();

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

            if (gameState.gameOver)
            {
                gameState = createGame();
            }
        }

        // cleanup
        // gRenderer.reset();

        std::cout << "Goodbye from Strongest Snake!\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal Error: " << e.what() << "\n";
        return -1;
    }
    catch (...)
    {
        std::cerr << "Unknown fatal error\n";
        return -1;
    }
}

Game::GameState createGame()
{
    std::cout << "Creating new game. GameOver: " << gameState.gameOver << "\n";

    Game::GameState state;
    srand(time(nullptr)); // Seeds rand

    int sx = floor(state.wCellCount / 2);
    int sy = floor(state.hCellCount / 2);
    state.snakeCoords.push_back({sx, sy});

    int fx = rand() % state.wCellCount;
    int fy = rand() % state.hCellCount;
    state.foodCoords[0] = fx;
    state.foodCoords[1] = fy;

    return state;
}
