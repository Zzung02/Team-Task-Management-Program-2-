// main.c
#include <windows.h>
#include <windowsx.h>
#include "app.h"

static HBRUSH g_brWhite = NULL;

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        if (!g_brWhite) g_brWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
        return (App_OnCreate(hWnd) == 0) ? 0 : -1;

    case WM_SIZE:
        App_OnSize(hWnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_DRAWITEM:
        return App_OnDrawItemWndProc(hWnd, wParam, lParam);

    case WM_MOUSEMOVE:
        return App_OnMouseMoveWndProc(hWnd, wParam, lParam);

    case WM_LBUTTONDOWN:
        App_OnLButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

        // ✅⭐ 핵심: 자식(EDIT/STATIC)에서 PostMessage로 보낸 클릭 좌표를 여기서 받아서
        //          똑같이 App_OnLButtonDown으로 전달해야 함
    case WM_APP_CHILDCLICK:
        App_OnLButtonDown(hWnd, (int)wParam, (int)lParam);
        return 0;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);
        return (INT_PTR)g_brWhite;
    }



    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        App_OnPaint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        App_OnDestroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nCmdShow)
{
    (void)hPrev; (void)cmd;

    const wchar_t CLASS_NAME[] = L"TTM_Window";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"Team-Task-Management-Program",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInst, NULL
    );

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
  