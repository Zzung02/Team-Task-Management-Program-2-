// main.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include "app.h"

// WM_APP_CHILDCLICK는 app.h에 정의되어 있어야 함(아래 app.h 수정 참고)

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


    case WM_LBUTTONDOWN:
        App_OnLButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;



    case WM_APP_CHILDCLICK:
    {
        int x = (int)wParam;
        int y = (int)lParam;
    
        App_OnLButtonDown(hWnd, x, y);
        return 0;
    }


    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkMode(hdc, TRANSPARENT);

        int id = GetDlgCtrlID(hCtrl);

        // ✅ TASK_ADD 과제 목록 STATIC: 902~905
        if (id == 902 || id == 903 || id == 904 || id == 905) {
            return (INT_PTR)g_brWhite;
        }

        return (INT_PTR)GetStockObject(WHITE_BRUSH);
    }


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

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrev; (void)lpCmdLine;

    if (!g_brWhite) g_brWhite = (HBRUSH)GetStockObject(WHITE_BRUSH);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"TTM_APP";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = g_brWhite;

    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Team Task Manager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
