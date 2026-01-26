#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>


//화면 
typedef enum {
    SCR_START = 0,    // 로그인 화면
    SCR_SIGNUP,       // 회원가입 화면
    SCR_MAIN = 1
    
} Screen;

static Screen  g_screen = SCR_START;

static HBITMAP g_bmpStart = NULL;
static HBITMAP g_bmpSignup = NULL;
static HBITMAP g_bmpMain = NULL;



static int g_clientW = 900;
static int g_clientH = 600;

// ---------------------------------------------------------
// BMP 로드 + 크기 얻기
// ---------------------------------------------------------
static HBITMAP LoadBmpFromFile(const wchar_t* path, int* outW, int* outH)
{
    HBITMAP hb = (HBITMAP)LoadImageW(
        NULL, path, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION
    );
    if (!hb) return NULL;

    BITMAP bm;
    GetObject(hb, sizeof(bm), &bm);

    if (outW) *outW = bm.bmWidth;
    if (outH) *outH = bm.bmHeight;

    return hb;
}



static void SetWindowClientSize(HWND hWnd, int clientW, int clientH)
{
    RECT rc = { 0, 0, clientW, clientH };
    DWORD style = (DWORD)GetWindowLongPtrW(hWnd, GWL_STYLE);
    DWORD ex = (DWORD)GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, ex);

    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    SetWindowPos(hWnd, NULL, 0, 0, winW, winH,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// ---------------------------------------------------------
// 더블버퍼링 페인트
// ---------------------------------------------------------
static void DrawBitmap(HDC dst, HBITMAP bmp, int w, int h)
{
    if (!bmp) return;

    HDC bmpDC = CreateCompatibleDC(dst);
    HBITMAP old = (HBITMAP)SelectObject(bmpDC, bmp);

    BitBlt(dst, 0, 0, w, h, bmpDC, 0, 0, SRCCOPY);

    SelectObject(bmpDC, old);
    DeleteDC(bmpDC);
}

static void DrawTextOverlay(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);

    // 제목 폰트
    HFONT hTitle = CreateFontW(
        28, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HFONT old = (HFONT)SelectObject(hdc, hTitle);

    if (g_screen == SCR_START) {
        const wchar_t* t1 = L"TEAM TASK MANAGER";
        const wchar_t* t2 = L"Click the START button area to continue";
        TextOutW(hdc, 30, 25, t1, lstrlenW(t1));
        SelectObject(hdc, old);
        DeleteObject(hTitle);

        HFONT hSub = CreateFontW(
            16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
        );
        old = (HFONT)SelectObject(hdc, hSub);
        TextOutW(hdc, 30, 65, t2, lstrlenW(t2));
        SelectObject(hdc, old);
        DeleteObject(hSub);
        return;
    }

    // MAIN 화면 텍스트 예시
    const wchar_t* m1 = L"MAIN SCREEN";
    TextOutW(hdc, 30, 25, m1, lstrlenW(m1));
    SelectObject(hdc, old);
    DeleteObject(hTitle);

    HFONT hSub = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    old = (HFONT)SelectObject(hdc, hSub);

    const wchar_t* m2 = L"Click BACK area to return to Start";
    TextOutW(hdc, 30, 65, m2, lstrlenW(m2));

    SelectObject(hdc, old);
    DeleteObject(hSub);
}

static void Paint(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP back = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBack = (HBITMAP)SelectObject(mem, back);

    // 배경 채우기
    HBRUSH bg = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(mem, &rc, bg);

    // 화면별 BMP
    if (g_screen == SCR_START) DrawBitmap(mem, g_bmpStart, w, h);
    else                       DrawBitmap(mem, g_bmpMain, w, h);

    // 텍스트 오버레이
    DrawTextOverlay(mem);

    // 화면에 한번에 출력
    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

    // 정리
    SelectObject(mem, oldBack);
    DeleteObject(back);
    DeleteDC(mem);
}

// ---------------------------------------------------------
// 클릭 영역 판정 (여기 좌표만 네 UI에 맞춰 바꾸면 됨)
// ---------------------------------------------------------
static int PtInRectXY(int x, int y, int x1, int y1, int x2, int y2)
{
    return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

// START 화면: "START 버튼" 영역 예시
// MAIN 화면:  "BACK 버튼" 영역 예시
static void HandleClick(HWND hWnd, int x, int y)
{
    if (g_screen == SCR_START) {

        // 🔥 회원가입 버튼 클릭 → 회원가입 화면
        if (PtInRectXY(x, y, 1000, 450, 1500, 520)) {
            g_screen = SCR_SIGNUP;
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }

        return;
    }

    if (g_screen == SCR_SIGNUP) {

        // 🔙 왼쪽 위 "회원가입" 버튼 누르면 다시 로그인 화면
        if (PtInRectXY(x, y, 40, 40, 220, 110)) {
            g_screen = SCR_START;
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }

        // 🔥 "생성하기" 버튼 → 다음 단계(일단 MAIN으로)
        if (PtInRectXY(x, y, 750, 750, 1050, 830)) {
            g_screen = SCR_MAIN;
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }
    }
}


// ---------------------------------------------------------
// WndProc
// ---------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        int w = 0, h = 0;

        g_bmpStart = LoadBmpFromFile(L"start.bmp", &w, &h);
        if (!g_bmpStart) {
            MessageBoxW(hWnd, L"start.bmp 로드 실패", L"Error", MB_ICONERROR);
            return -1;
        }
        SetWindowClientSize(hWnd, w, h);

        g_bmpSignup = LoadBmpFromFile(L"signup.bmp", NULL, NULL);
        g_bmpMain = LoadBmpFromFile(L"main.bmp", NULL, NULL);

        return 0;
    }


    case WM_LBUTTONDOWN:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        HandleClick(hWnd, x, y);
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Paint(hWnd, hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_bmpStart) DeleteObject(g_bmpStart);
        if (g_bmpMain)  DeleteObject(g_bmpMain);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ---------------------------------------------------------
// WinMain
// ---------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int show)
{
    (void)hPrev; (void)cmd;

    const wchar_t* CLASS_NAME = L"TTM_BG_UI";

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassW(&wc)) return 0;

    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"Team Task Manager",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        g_clientW, g_clientH,
        NULL, NULL, hInst, NULL
    );
    if (!hWnd) return 0;

    ShowWindow(hWnd, show);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
