// main.c  (원샷 덮어쓰기)
// - STATIC(내팀 슬롯) 배경 흰색 처리 (WM_CTLCOLORSTATIC)
// - 윈도우 클래스 배경도 흰색으로 지정
// - 나머지 흐름은 기존 그대로

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
        if (App_OnDrawItem(hWnd, (const DRAWITEMSTRUCT*)lParam))
            return TRUE;
        break;


    case WM_MOUSEMOVE:
        App_OnMouseMove(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;

    case WM_LBUTTONDOWN:
        App_OnLButtonDown(hWnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;


    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(255, 255, 255));
        return (LRESULT)(g_brWhite ? g_brWhite : (HBRUSH)GetStockObject(WHITE_BRUSH));
    }

    // ✅ EDIT는 그대로 두되, 흰 배경 원하면 여기서 처리 가능
    case WM_CTLCOLOREDIT:
    {
        HDC hdc = (HDC)wParam;
        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(255, 255, 255));
        return (LRESULT)(g_brWhite ? g_brWhite : (HBRUSH)GetStockObject(WHITE_BRUSH));
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
        // g_brWhite는 stock object라 DeleteObject 하면 안 됨
        g_brWhite = NULL;
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

    // ✅ 창 기본 배경을 흰색으로
    wc.hbrBackground = g_brWhite;

    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(
        0, wc.lpszClassName, L"Team Task Manager",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInst, NULL
    );

    ShowWindow(hWnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}