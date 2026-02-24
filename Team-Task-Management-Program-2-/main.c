// main.c
#include <windows.h>
#include <windowsx.h>
#include "app.h"

#ifndef WM_APP_CHILDCLICK
#define WM_APP_CHILDCLICK (WM_APP + 10)
#endif

static HBRUSH g_brWhite = NULL;
static HBRUSH g_brTitleBg = NULL;   // 제목 Edit 배경용

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        if (!g_brWhite) g_brWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);
        if (!g_brTitleBg) g_brTitleBg = CreateSolidBrush(RGB(255, 255, 255)); // ✅ 필요시 변경
        return (App_OnCreate(hWnd) == 0) ? 0 : -1;

    case WM_SIZE:
        App_OnSize(hWnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        App_OnPaint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // ✅ 우리가 전체 그리기 하니까 배경 지우기 막기(하얀 깜빡임 방지)

    case WM_DRAWITEM:
        return App_OnDrawItemWndProc(hWnd, wParam, lParam);

    case WM_MOUSEMOVE:
        return App_OnMouseMoveWndProc(hWnd, wParam, lParam);

    case WM_LBUTTONDOWN:
        App_OnLButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    // ✅ 자식(EDIT/STATIC)에서 PostMessage로 보낸 클릭 좌표 전달
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

    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        int id = GetDlgCtrlID(hCtrl);

        if (id == 8801) { // ✅ 제목칸
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(220, 234, 247));
            SetTextColor(hdc, RGB(0, 0, 0));
            return (INT_PTR)g_brTitleBg;
        }
        break;
    }

    case WM_DESTROY:
        if (g_brTitleBg) { DeleteObject(g_brTitleBg); g_brTitleBg = NULL; }
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