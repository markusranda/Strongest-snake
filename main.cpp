#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <cstdio>

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

        HBRUSH brush = CreateSolidBrush(RGB(30, 30, 120)); // dark blue background
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
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

    RECT screen;
    GetWindowRect(GetDesktopWindow(), &screen);

    int width = 800;
    int height = 600;
    int x = (screen.right - width) / 2;
    int y = (screen.bottom - height) / 2;

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Strongest Snake",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, width, height,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
    {
        std::cerr << "Window creation failed!\n";
        return 1;
    }

    std::cout << "Window created successfully!\n";

    // Message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    std::cout << "Goodbye from Strongest Snake!\n";
    return 0;
}
