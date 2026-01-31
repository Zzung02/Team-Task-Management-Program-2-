
#include <windows.h>
#include <windowsx.h>
#include "app.h"

// ✅ Edit 배경 브러시(하얀색) - WM_DESTROY에서 해제
static HBRUSH g_hbrWhiteEdit = NULL;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // App_OnCreate 안에서 SwitchScreen -> ResizeToBitmap -> InvalidateRect 하니까
        // 여기서 굳이 InvalidateRect 먼저 할 필요는 없음(해도 문제는 없음)
        if (App_OnCreate(hWnd) != 0) return -1;
        return 0;

    case WM_SIZE:
        App_OnSize(hWnd, LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        App_OnLButtonDown(hWnd, x, y);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        App_OnPaint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    // ✅ Edit(텍스트박스) 색/배경 처리
    case WM_CTLCOLOREDIT:
    {
        HDC hdcEdit = (HDC)wParam;

        SetTextColor(hdcEdit, RGB(0, 0, 0));
        SetBkColor(hdcEdit, RGB(255, 255, 255));

        if (!g_hbrWhiteEdit)
            g_hbrWhiteEdit = CreateSolidBrush(RGB(255, 255, 255));

        return (INT_PTR)g_hbrWhiteEdit;
    }

    // ✅ Static(라벨)이나 읽기전용 Edit가 Static처럼 처리될 때 대비
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = (HDC)wParam;

        SetTextColor(hdc, RGB(0, 0, 0));
        SetBkColor(hdc, RGB(255, 255, 255));

        if (!g_hbrWhiteEdit)
            g_hbrWhiteEdit = CreateSolidBrush(RGB(255, 255, 255));

        return (INT_PTR)g_hbrWhiteEdit;
    }

    case WM_PARENTNOTIFY:
        // 자식(Edit 등) 클릭도 last click 잡기
        if (LOWORD(wParam) == WM_LBUTTONDOWN) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            App_OnLButtonDown(hWnd, x, y);
            return 0;
        }
        break;

    case WM_CLOSE:
        // ✅ 로그인 화면이 아니면 "로그인 화면으로만" 보내고 종료는 막기
        if (g_screen != SCR_START) {
            App_GoToStart(hWnd);
            return 0;
        }

        // ✅ 로그인 화면이면 진짜 종료
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        App_OnDestroy();

        if (g_hbrWhiteEdit) {
            DeleteObject(g_hbrWhiteEdit);
            g_hbrWhiteEdit = NULL;
        }

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    (void)hPrev;
    (void)cmd;

    const wchar_t* CLASS_NAME = L"TTM_3SCREENS";

    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc)) return 0;

    HWND hWnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Team Task Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL, NULL, hInst, NULL
    );
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
