#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>   // CommandLineToArgvW
#include <cwchar>
#include <cstdlib>
#include "Core/App.h"
#include "imgui/imgui.h"

static App g_app;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
            g_app.Engine().device->Resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    // --- Input forwarding ---
    // Keyboard: always forward to game Input (ImGui gets it via WndProcHandler above)
    // Only block game input when an ImGui text input is focused
    case WM_KEYDOWN:
        if (!ImGui::GetIO().WantTextInput)
            g_app.Engine().input->OnKeyDown(wParam);
        return 0;
    case WM_KEYUP:
        if (!ImGui::GetIO().WantTextInput)
            g_app.Engine().input->OnKeyUp(wParam);
        return 0;
    case WM_MOUSEMOVE:
        g_app.Engine().input->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        if (!ImGui::GetIO().WantCaptureMouse)
            g_app.Engine().input->OnMouseButton(0, true);
        return 0;
    case WM_LBUTTONUP:
        g_app.Engine().input->OnMouseButton(0, false);
        return 0;
    case WM_RBUTTONDOWN:
        if (!ImGui::GetIO().WantCaptureMouse)
            g_app.Engine().input->OnMouseButton(1, true);
        return 0;
    case WM_RBUTTONUP:
        g_app.Engine().input->OnMouseButton(1, false);
        return 0;
    case WM_MBUTTONDOWN:
        g_app.Engine().input->OnMouseButton(2, true);
        return 0;
    case WM_MBUTTONUP:
        g_app.Engine().input->OnMouseButton(2, false);
        return 0;
    case WM_MOUSEWHEEL:
        if (!ImGui::GetIO().WantCaptureMouse)
            g_app.Engine().input->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / 120.0f);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // --- Command line parsing (optional) ---
    // Supported flags:
    //   -t "Title"   window title (default "Isometric Client")
    //   -w <int>     window width  (default 1280)
    //   -h <int>     window height (default 720)
    //   -x <int>     initial X position (default CW_USEDEFAULT)
    //   -y <int>     initial Y position (default CW_USEDEFAULT)
    int winW = 1280, winH = 720;
    int winX = CW_USEDEFAULT, winY = CW_USEDEFAULT;
    std::wstring title = L"Isometric Client";
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc - 1; ++i) {
                if      (!wcscmp(argv[i], L"-t")) title = argv[++i];
                else if (!wcscmp(argv[i], L"-w")) winW  = _wtoi(argv[++i]);
                else if (!wcscmp(argv[i], L"-h")) winH  = _wtoi(argv[++i]);
                else if (!wcscmp(argv[i], L"-x")) winX  = _wtoi(argv[++i]);
                else if (!wcscmp(argv[i], L"-y")) winY  = _wtoi(argv[++i]);
            }
            LocalFree(argv);
        }
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"IsometricClient";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"IsometricClient", title.c_str(),
        WS_OVERLAPPEDWINDOW,
        winX, winY, winW, winH,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_app.Init(hwnd, hInstance, winW, winH)) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    Timer timer;
    timer.Init();

    MSG msg = {};
    bool running = true;
    while (running) {
        g_app.Engine().input->NewFrame();
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        float dt = timer.Tick();
        g_app.PollNetwork();   // drain IOCP completions before scene update
        g_app.Update(dt);
        g_app.Render();
    }

    g_app.Shutdown();
    return (int)msg.wParam;
}
