#define _CRT_SECURE_NO_WARNINGS
#include "board.h"

#include "app.h"        // App_CreateEdit 사용하려고 포함
#include "ui_coords.h"
#include "ui_layout.h"

#include <wchar.h>
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------
// Board Edit handles (board.c가 전담 관리)
// ---------------------------------------------------------
static HWND g_edBdSearch = NULL;
static HWND g_edBdTitle = NULL;
static HWND g_edBdContent = NULL;

// ---------------------------------------------------------
// Storage file (간단 저장)
// ---------------------------------------------------------
static const wchar_t* kBoardFile = L"board.txt";

// ---------------------------------------------------------
// Local hit test (scaled)
// ---------------------------------------------------------
static int HitScaledLocal(int x1, int y1, int x2, int y2, int x, int y)
{
    int L = SX(x1), T = SY(y1), R = SX(x2), B = SY(y2);

    RECT rc;
    rc.left = (L < R) ? L : R;
    rc.right = (L < R) ? R : L;
    rc.top = (T < B) ? T : B;
    rc.bottom = (T < B) ? B : T;

    POINT pt = { x, y };
    return PtInRect(&rc, pt);
}

// ---------------------------------------------------------
// Load / Save
// ---------------------------------------------------------
void Board_LoadFromFile(void)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, kBoardFile, L"r, ccs=UTF-8");
    if (!fp) return;

    wchar_t title[512] = { 0 };
    wchar_t content[4096] = { 0 };

    // 1줄: 제목
    fgetws(title, 512, fp);

    // 나머지: 내용
    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        if (wcslen(content) + wcslen(line) < 4090)
            wcscat_s(content, 4096, line);
        else break;
    }
    fclose(fp);

    // 제목 끝 개행 제거
    size_t n = wcslen(title);
    while (n > 0 && (title[n - 1] == L'\n' || title[n - 1] == L'\r')) {
        title[n - 1] = 0;
        n--;
    }

    if (g_edBdTitle)   SetWindowTextW(g_edBdTitle, title);
    if (g_edBdContent) SetWindowTextW(g_edBdContent, content);
}

void Board_SaveToFile(void)
{
    if (!g_edBdTitle || !g_edBdContent) return;

    wchar_t title[512] = { 0 };
    wchar_t content[4096] = { 0 };

    GetWindowTextW(g_edBdTitle, title, 512);
    GetWindowTextW(g_edBdContent, content, 4096);

    FILE* fp = NULL;
    _wfopen_s(&fp, kBoardFile, L"w, ccs=UTF-8");
    if (!fp) return;

    fwprintf(fp, L"%s\n", title);
    fwprintf(fp, L"%s", content);
    fclose(fp);
}

// ---------------------------------------------------------
// Create / Destroy / Relayout
// ---------------------------------------------------------
void Board_DestroyControls(void)
{
    if (g_edBdSearch) { DestroyWindow(g_edBdSearch);  g_edBdSearch = NULL; }
    if (g_edBdTitle) { DestroyWindow(g_edBdTitle);   g_edBdTitle = NULL; }
    if (g_edBdContent) { DestroyWindow(g_edBdContent); g_edBdContent = NULL; }
}

void Board_CreateControls(HWND hWnd)
{
    // 기존 있으면 제거
    Board_DestroyControls();

    // ✅ app.c에서 공개한 App_CreateEdit 재사용
    g_edBdSearch = App_CreateEdit(hWnd, 1001, 0);
    g_edBdTitle = App_CreateEdit(hWnd, 1002, 0);
    g_edBdContent = App_CreateEdit(hWnd, 1003, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);

    Board_RelayoutControls(hWnd);
    Board_LoadFromFile();
}

void Board_RelayoutControls(HWND hWnd)
{
    (void)hWnd;
    if (!g_edBdSearch || !g_edBdTitle || !g_edBdContent) return;

    ShowWindow(g_edBdSearch, SW_SHOW);
    ShowWindow(g_edBdTitle, SW_SHOW);
    ShowWindow(g_edBdContent, SW_SHOW);

    // MoveEdit는 (x1,y1,x2,y2) 넣는 버전으로 사용
    MoveEdit(g_edBdSearch, SX(R_BD_SEARCH_X1), SY(R_BD_SEARCH_Y1),
        SX(R_BD_SEARCH_X2), SY(R_BD_SEARCH_Y2), 0, 0, 0, 0);

    MoveEdit(g_edBdTitle, SX(R_BD_TITLE_X1), SY(R_BD_TITLE_Y1),
        SX(R_BD_TITLE_X2), SY(R_BD_TITLE_Y2), 0, 0, 0, 0);

    MoveEdit(g_edBdContent, SX(R_BD_CONTENT_X1), SY(R_BD_CONTENT_Y1),
        SX(R_BD_CONTENT_X2), SY(R_BD_CONTENT_Y2), 0, 0, 0, 0);
}

// ---------------------------------------------------------
// Click handling
// ---------------------------------------------------------
int Board_OnClick(HWND hWnd, int x, int y)
{
    // 등록 버튼: 입력 초기화
    if (HitScaledLocal(R_BD_BTN_ADD_X1, R_BD_BTN_ADD_Y1, R_BD_BTN_ADD_X2, R_BD_BTN_ADD_Y2, x, y))
    {
        if (g_edBdTitle)   SetWindowTextW(g_edBdTitle, L"");
        if (g_edBdContent) SetWindowTextW(g_edBdContent, L"");
        if (g_edBdTitle)   SetFocus(g_edBdTitle);
        return 1;
    }

    // 수정 버튼: 내용 포커스
    if (HitScaledLocal(R_BD_BTN_EDIT_X1, R_BD_BTN_EDIT_Y1, R_BD_BTN_EDIT_X2, R_BD_BTN_EDIT_Y2, x, y))
    {
        if (g_edBdContent) SetFocus(g_edBdContent);
        return 1;
    }

    // 완료 버튼: 저장
    if (HitScaledLocal(R_BD_BTN_DONE_X1, R_BD_BTN_DONE_Y1, R_BD_BTN_DONE_X2, R_BD_BTN_DONE_Y2, x, y))
    {
        Board_SaveToFile();
        MessageBoxW(hWnd, L"게시글 저장 완료!", L"게시판", MB_OK | MB_ICONINFORMATION);
        return 1;
    }

    return 0;
}