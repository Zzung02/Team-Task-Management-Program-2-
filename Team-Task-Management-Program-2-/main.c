#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>   
#include "app.h"

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // App_OnCreate에서 BMP 로드 + 창크기 조정 + 컨트롤 생성까지 끝냄
        if (App_OnCreate(hWnd) != 0) return -1;
        return 0;

    case WM_SIZE:
    {
        int w = LOWORD(lParam);
        int h = HIWORD(lParam);

        // 최소화(0,0)로 들어오는 경우가 있어서 방어
        if (w > 0 && h > 0) {
            App_OnSize(hWnd, w, h);
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        // GET_X/Y_LPARAM이 음수 좌표도 안전하게 처리
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        App_OnLButtonDown(hWnd, x, y);
        return 0;
    }

    case WM_ERASEBKGND:
        // App_OnPaint에서 백버퍼로 전체를 그리고 있으니
        // 기본 배경지우기 막아서 깜빡임 줄이기
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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // 안전용(실제는 WM_ERASEBKGND에서 막음)

    if (!RegisterClassW(&wc)) return 0;

    // 처음 크기는 대충(어차피 App_OnCreate에서 BMP로 맞춰줄 거임)
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
    UpdateWindow(hWnd); // 첫 PAINT 빠르게

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
