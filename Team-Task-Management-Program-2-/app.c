// app.c
#include "app.h"
#include "ui_coords.h"
#include "ui_layout.h"
#include "bmp_loader.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <windowsx.h>

static void RelayoutControls(HWND hWnd);
Screen g_screen = SCR_START;

typedef enum { OVR_NONE = 0, OVR_DEADLINE = 1 } Overlay;
static Overlay g_overlay = OVR_NONE;

// ✅ 오버레이(마감 패널) 영역 + X버튼 영역 (원본 좌표 기준 값으로 잡아야 함)
static RECT g_rcDeadlinePanel = { 0,0,0,0 };
static RECT g_rcDeadlineClose = { 0,0,0,0 };

static HFONT GetUIFont(void);



// 오른쪽 위 "로그인" 버튼 고정 Rect
static RECT g_rcTopLogin = { 0,0,0,0 };

// 버튼 크기/여백(너 UI에 맞게 조절 가능)
#define TOP_LOGIN_W   260   // 버튼 가로
#define TOP_LOGIN_H   70    // 버튼 세로
#define TOP_LOGIN_MX  30    // 오른쪽 여백
#define TOP_LOGIN_MY  25    // 위쪽 여백


static void UpdateTopLoginRect(void)
{
    // 클라이언트 기준으로 오른쪽 위 고정
    g_rcTopLogin.right = g_clientW - TOP_LOGIN_MX;
    g_rcTopLogin.left = g_rcTopLogin.right - TOP_LOGIN_W;
    g_rcTopLogin.top = TOP_LOGIN_MY;
    g_rcTopLogin.bottom = g_rcTopLogin.top + TOP_LOGIN_H;
}

// Edit(텍스트필드) 클릭도 Last Click으로 잡히게 하는 훅

static const wchar_t* PROP_OLDPROC = L"TTM_OLD_EDIT_PROC";

static LRESULT CALLBACK EditHookProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        HWND parent = GetParent(hEdit);

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(parent, &pt);

        App_OnLButtonDown(parent, pt.x, pt.y);
    }

    WNDPROC oldProc = (WNDPROC)GetPropW(hEdit, PROP_OLDPROC);
    if (!oldProc) return DefWindowProcW(hEdit, msg, wParam, lParam);
    return CallWindowProcW(oldProc, hEdit, msg, wParam, lParam);
}

// ✅ 폼(패널)처럼 보이는 화면들의 패널 영역을 Screen별로 얻기
static int GetPanelRectForScreen(Screen s, RECT* outRc)
{
    if (!outRc) return 0;

    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    switch (s)
    {
    case SCR_TEAM_CREATE: x1 = R_TC_PANEL_X1; y1 = R_TC_PANEL_Y1; x2 = R_TC_PANEL_X2; y2 = R_TC_PANEL_Y2; break;
    case SCR_TEAM_JOIN:   x1 = R_TJ_PANEL_X1; y1 = R_TJ_PANEL_Y1; x2 = R_TJ_PANEL_X2; y2 = R_TJ_PANEL_Y2; break;
    default: return 0;
    }

    outRc->left = SX(x1);
    outRc->top = SY(y1);
    outRc->right = SX(x2);
    outRc->bottom = SY(y2);
    return 1;
}




// =========================================================
// [BMP 파일명]
// =========================================================
const wchar_t* BMP_START = L"start.bmp";
const wchar_t* BMP_SIGNUP = L"signup.bmp";
const wchar_t* BMP_MAIN = L"main.bmp";
const wchar_t* BMP_FINDPW = L"findpw.bmp";
const wchar_t* BMP_DEADLINE = L"deadline.bmp";
const wchar_t* BMP_TODO = L"todo.bmp";
const wchar_t* BMP_MYTEAM = L"myteam.bmp";
const wchar_t* BMP_DONE = L"done.bmp";
const wchar_t* BMP_TEAM_CREATE = L"team_create.bmp";
const wchar_t* BMP_TEAM_JOIN = L"team_join.bmp";
const wchar_t* BMP_TASK_ADD = L"task_add.bmp";
const wchar_t* BMP_BOARD = L"board.bmp";

// =========================================================
// [BMP 핸들]
// =========================================================
HBITMAP g_bmpStart = NULL;
HBITMAP g_bmpSignup = NULL;
HBITMAP g_bmpMain = NULL;
HBITMAP g_bmpFindPw = NULL;
HBITMAP g_bmpDeadline = NULL;
HBITMAP g_bmpTodo = NULL;
HBITMAP g_bmpMyTeam = NULL;
HBITMAP g_bmpDone = NULL;
HBITMAP g_bmpTeamCreate = NULL;
HBITMAP g_bmpTeamJoin = NULL;
HBITMAP g_bmpTaskAdd = NULL;
HBITMAP g_bmpBoard = NULL;




// =========================================================
// [디버그/클라이언트 크기]
// =========================================================
int g_lastX = -1, g_lastY = -1;
int g_clientW = 0, g_clientH = 0;

// =========================================================
// [공통 폰트]
// =========================================================
static HFONT g_uiFont = NULL;

static HFONT GetUIFont(void)
{
    if (!g_uiFont) {
        g_uiFont = CreateFontW(
            20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
        );
    }
    return g_uiFont;
}

// =========================================================
// [텍스트 필드(Edit 컨트롤)]
// =========================================================
static HWND g_edStartId = NULL;
static HWND g_edStartPw = NULL;

static HWND g_edSignName = NULL;
static HWND g_edSignId = NULL;
static HWND g_edSignPw = NULL;

static HWND g_edFindName = NULL;
static HWND g_edFindId = NULL;
static HWND g_edFindResult = NULL;


static HWND g_edTcTeamName = NULL;   
static HWND g_edTcTaskName = NULL;  
static HWND g_edTcJoinCode = NULL;   

static HWND g_edTjTeamName = NULL;   // 팀참여: 팀명
static HWND g_edTjJoinCode = NULL;   // 팀참여: 참여코드

static HWND g_edMainTeamName = NULL;  // 메인: 팀명
static HWND g_edMainTaskName = NULL;  // 메인: 과제명j

static HWND g_edSearch = NULL;   // ✅ 조회(검색) 텍스트박스
static HWND g_edTaSubDetail = NULL;  // ✅ 상세사항(한 줄)

static HWND g_edBdSearch = NULL;   // 조회
static HWND g_edBdTitle = NULL;   // 제목
static HWND g_edBdContent = NULL;   // 내용(멀티라인)
static HWND g_edBdDetail = NULL;   // 상세사항(한 줄)


// ---------------------------------------------------------
// 내부 유틸
// ---------------------------------------------------------

static void DestroyAllEdits(void)
{
    // START
    if (g_edStartId) { RemovePropW(g_edStartId, PROP_OLDPROC); DestroyWindow(g_edStartId); g_edStartId = NULL; }
    if (g_edStartPw) { RemovePropW(g_edStartPw, PROP_OLDPROC); DestroyWindow(g_edStartPw); g_edStartPw = NULL; }

    // SIGNUP
    if (g_edSignName) { RemovePropW(g_edSignName, PROP_OLDPROC); DestroyWindow(g_edSignName); g_edSignName = NULL; }
    if (g_edSignId) { RemovePropW(g_edSignId, PROP_OLDPROC); DestroyWindow(g_edSignId);   g_edSignId = NULL; }
    if (g_edSignPw) { RemovePropW(g_edSignPw, PROP_OLDPROC); DestroyWindow(g_edSignPw);   g_edSignPw = NULL; }

    // FINDPW
    if (g_edFindName) { RemovePropW(g_edFindName, PROP_OLDPROC); DestroyWindow(g_edFindName);   g_edFindName = NULL; }
    if (g_edFindId) { RemovePropW(g_edFindId, PROP_OLDPROC); DestroyWindow(g_edFindId);     g_edFindId = NULL; }
    if (g_edFindResult) { RemovePropW(g_edFindResult, PROP_OLDPROC); DestroyWindow(g_edFindResult); g_edFindResult = NULL; }

    // TEAM_CREATE
    if (g_edTcTeamName) { RemovePropW(g_edTcTeamName, PROP_OLDPROC); DestroyWindow(g_edTcTeamName); g_edTcTeamName = NULL; }
    if (g_edTcTaskName) { RemovePropW(g_edTcTaskName, PROP_OLDPROC); DestroyWindow(g_edTcTaskName); g_edTcTaskName = NULL; }
    if (g_edTcJoinCode) { RemovePropW(g_edTcJoinCode, PROP_OLDPROC); DestroyWindow(g_edTcJoinCode); g_edTcJoinCode = NULL; }

    // TEAM_JOIN
    if (g_edTjTeamName) { RemovePropW(g_edTjTeamName, PROP_OLDPROC); DestroyWindow(g_edTjTeamName); g_edTjTeamName = NULL; }
    if (g_edTjJoinCode) { RemovePropW(g_edTjJoinCode, PROP_OLDPROC); DestroyWindow(g_edTjJoinCode); g_edTjJoinCode = NULL; }


    // TASK_ADD
    if (g_edTaItem1) { RemovePropW(g_edTaItem1, PROP_OLDPROC); DestroyWindow(g_edTaItem1);  g_edTaItem1 = NULL; }
    if (g_edTaItem2) { RemovePropW(g_edTaItem2, PROP_OLDPROC); DestroyWindow(g_edTaItem2);  g_edTaItem2 = NULL; }
    if (g_edTaItem3) { RemovePropW(g_edTaItem3, PROP_OLDPROC); DestroyWindow(g_edTaItem3);  g_edTaItem3 = NULL; }
    if (g_edTaItem4) { RemovePropW(g_edTaItem4, PROP_OLDPROC); DestroyWindow(g_edTaItem4);  g_edTaItem4 = NULL; }
    if (g_edTaTitle) { RemovePropW(g_edTaTitle, PROP_OLDPROC); DestroyWindow(g_edTaTitle);  g_edTaTitle = NULL; }
    if (g_edTaDetail) { RemovePropW(g_edTaDetail, PROP_OLDPROC); DestroyWindow(g_edTaDetail); g_edTaDetail = NULL; }
    if (g_edTaFile) { RemovePropW(g_edTaFile, PROP_OLDPROC); DestroyWindow(g_edTaFile);   g_edTaFile = NULL; }
    if (g_edSearch) { RemovePropW(g_edSearch, PROP_OLDPROC); DestroyWindow(g_edSearch); g_edSearch = NULL; }
    if (g_edTaSubDetail) { RemovePropW(g_edTaSubDetail, PROP_OLDPROC);DestroyWindow(g_edTaSubDetail);g_edTaSubDetail = NULL;
    }

    // BOARD
    if (g_edBdSearch) { RemovePropW(g_edBdSearch, PROP_OLDPROC); DestroyWindow(g_edBdSearch);  g_edBdSearch = NULL; }
    if (g_edBdTitle) { RemovePropW(g_edBdTitle, PROP_OLDPROC); DestroyWindow(g_edBdTitle);   g_edBdTitle = NULL; }
    if (g_edBdContent) { RemovePropW(g_edBdContent, PROP_OLDPROC); DestroyWindow(g_edBdContent); g_edBdContent = NULL; }
    if (g_edBdDetail) { RemovePropW(g_edBdDetail, PROP_OLDPROC); DestroyWindow(g_edBdDetail);  g_edBdDetail = NULL; }

}

static HWND CreateEdit(HWND parent, int ctrlId, DWORD extraStyle)
{
    DWORD style = WS_CHILD | WS_VISIBLE | ES_LEFT | extraStyle;

    // 멀티라인이 아니면 가로 자동 스크롤(한줄)
    if ((extraStyle & ES_MULTILINE) == 0) {
        style |= ES_AUTOHSCROLL;
    }

    HWND h = CreateWindowExW(
        0, L"EDIT", L"",
        style,
        10, 10, 100, 30,
        parent, (HMENU)(INT_PTR)ctrlId,
        GetModuleHandleW(NULL), NULL
    );

    HFONT f = GetUIFont();
    if (f) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);

    // ✅ 클릭 훅
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)EditHookProc);
    SetPropW(h, PROP_OLDPROC, (HANDLE)oldProc);

    return h;
}




static void CreateControlsForScreen(HWND hWnd, Screen s)
{
    DestroyAllEdits();

    switch (s)
    {
    case SCR_START:
        g_edStartId = CreateEdit(hWnd, 101, 0);
        g_edStartPw = CreateEdit(hWnd, 102, ES_PASSWORD);
        RelayoutControls(hWnd);
        return;

    case SCR_SIGNUP:
        g_edSignName = CreateEdit(hWnd, 201, 0);
        g_edSignId = CreateEdit(hWnd, 202, 0);
        g_edSignPw = CreateEdit(hWnd, 203, ES_PASSWORD);
        RelayoutControls(hWnd);
        return;

    case SCR_FINDPW:
        g_edFindName = CreateEdit(hWnd, 301, 0);
        g_edFindId = CreateEdit(hWnd, 302, 0);
        g_edFindResult = CreateEdit(hWnd, 303, ES_MULTILINE | ES_AUTOVSCROLL);
        RelayoutControls(hWnd);
        return;

    case SCR_TEAM_CREATE:
        g_edTcTeamName = CreateEdit(hWnd, 401, 0);
        g_edTcTaskName = CreateEdit(hWnd, 402, 0);
        g_edTcJoinCode = CreateEdit(hWnd, 403, 0);
        RelayoutControls(hWnd);
        return;

    case SCR_TEAM_JOIN:
        g_edTjTeamName = CreateEdit(hWnd, 501, 0);
        g_edTjJoinCode = CreateEdit(hWnd, 502, 0);
        RelayoutControls(hWnd);
        return;

    case SCR_MAIN:
        g_edMainTeamName = CreateEdit(hWnd, 601, 0);
        g_edMainTaskName = CreateEdit(hWnd, 602, 0);
        g_edSearch = CreateEdit(hWnd, 603, 0);
        RelayoutControls(hWnd);
        return;

    case SCR_TASK_ADD:
        g_edTaItem1 = CreateEdit(hWnd, 701, ES_READONLY);
        g_edTaItem2 = CreateEdit(hWnd, 702, ES_READONLY);
        g_edTaItem3 = CreateEdit(hWnd, 703, ES_READONLY);
        g_edTaItem4 = CreateEdit(hWnd, 704, ES_READONLY);
        g_edTaSubDetail = CreateEdit(hWnd, 708, 0);

        g_edTaTitle = CreateEdit(hWnd, 705, 0);
        g_edTaDetail = CreateEdit(hWnd, 706, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
        g_edTaFile = CreateEdit(hWnd, 707, 0);

        g_edSearch = CreateEdit(hWnd, 708, 0);   // ✅ 조회 텍스트박스(과제등록 상단)
        g_edTaSubDetail = CreateEdit(hWnd, 708, 0);
        RelayoutControls(hWnd);
        return;

    case SCR_BOARD:
        g_edBdSearch = CreateEdit(hWnd, 801, 0); // 한줄
        g_edBdTitle = CreateEdit(hWnd, 802, 0); // 한줄
        g_edBdContent = CreateEdit(
            hWnd, 803,
            ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL
        );

        g_edBdDetail = CreateEdit(hWnd, 804, 0); // 한줄

        RelayoutControls(hWnd);
        return;

    }
}

static void RelayoutControls(HWND hWnd)
{
    (void)hWnd;

    // 전부 숨김
    if (g_edStartId)     ShowWindow(g_edStartId, SW_HIDE);
    if (g_edStartPw)     ShowWindow(g_edStartPw, SW_HIDE);

    if (g_edSignName)    ShowWindow(g_edSignName, SW_HIDE);
    if (g_edSignId)      ShowWindow(g_edSignId, SW_HIDE);
    if (g_edSignPw)      ShowWindow(g_edSignPw, SW_HIDE);

    if (g_edFindName)    ShowWindow(g_edFindName, SW_HIDE);
    if (g_edFindId)      ShowWindow(g_edFindId, SW_HIDE);
    if (g_edFindResult)  ShowWindow(g_edFindResult, SW_HIDE);

    if (g_edTcTeamName)  ShowWindow(g_edTcTeamName, SW_HIDE);
    if (g_edTcTaskName)  ShowWindow(g_edTcTaskName, SW_HIDE);
    if (g_edTcJoinCode)  ShowWindow(g_edTcJoinCode, SW_HIDE);

    if (g_edTjTeamName)  ShowWindow(g_edTjTeamName, SW_HIDE);
    if (g_edTjJoinCode)  ShowWindow(g_edTjJoinCode, SW_HIDE);

    if (g_edMainTeamName) ShowWindow(g_edMainTeamName, SW_HIDE);
    if (g_edMainTaskName) ShowWindow(g_edMainTaskName, SW_HIDE);

   

    if (g_edTaItem1)  ShowWindow(g_edTaItem1, SW_HIDE);
    if (g_edTaItem2)  ShowWindow(g_edTaItem2, SW_HIDE);
    if (g_edTaItem3)  ShowWindow(g_edTaItem3, SW_HIDE);
    if (g_edTaItem4)  ShowWindow(g_edTaItem4, SW_HIDE);
    if (g_edTaTitle)  ShowWindow(g_edTaTitle, SW_HIDE);
    if (g_edTaDetail) ShowWindow(g_edTaDetail, SW_HIDE);
    if (g_edTaFile)   ShowWindow(g_edTaFile, SW_HIDE);
    if (g_edSearch)   ShowWindow(g_edSearch, SW_HIDE);


    if (g_edBdSearch)  ShowWindow(g_edBdSearch, SW_HIDE);
    if (g_edBdTitle)   ShowWindow(g_edBdTitle, SW_HIDE);
    if (g_edBdContent) ShowWindow(g_edBdContent, SW_HIDE);
    if (g_edBdDetail)  ShowWindow(g_edBdDetail, SW_HIDE);



    // =========================
    // START
    // =========================
    if (g_screen == SCR_START) {
        ShowWindow(g_edStartId, SW_SHOW);
        ShowWindow(g_edStartPw, SW_SHOW);

        MoveEdit(g_edStartId, SX(R_START_ID_X1), SY(R_START_ID_Y1), SX(R_START_ID_X2), SY(R_START_ID_Y2), 0, 0, 0, 0);
        MoveEdit(g_edStartPw, SX(R_START_PW_X1), SY(R_START_PW_Y1), SX(R_START_PW_X2), SY(R_START_PW_Y2), 0, 0, 0, 0);
        return;
    }

    // =========================
    // SIGNUP
    // =========================
    if (g_screen == SCR_SIGNUP) {
        ShowWindow(g_edSignName, SW_SHOW);
        ShowWindow(g_edSignId, SW_SHOW);
        ShowWindow(g_edSignPw, SW_SHOW);

        MoveEdit(g_edSignName, SX(R_SIGN_NAME_X1), SY(R_SIGN_NAME_Y1), SX(R_SIGN_NAME_X2), SY(R_SIGN_NAME_Y2), 0, 0, 0, 0);
        MoveEdit(g_edSignId, SX(R_SIGN_ID_X1), SY(R_SIGN_ID_Y1), SX(R_SIGN_ID_X2), SY(R_SIGN_ID_Y2), 0, 0, 0, 0);
        MoveEdit(g_edSignPw, SX(R_SIGN_PW_X1), SY(R_SIGN_PW_Y1), SX(R_SIGN_PW_X2), SY(R_SIGN_PW_Y2), 0, 0, 0, 0);
        return;
    }

    // =========================
    // FINDPW
    // =========================
    if (g_screen == SCR_FINDPW) {
        ShowWindow(g_edFindName, SW_SHOW);
        ShowWindow(g_edFindId, SW_SHOW);
        ShowWindow(g_edFindResult, SW_SHOW);

        MoveEdit(g_edFindName, SX(R_FIND_NAME_X1), SY(R_FIND_NAME_Y1), SX(R_FIND_NAME_X2), SY(R_FIND_NAME_Y2), 0, 0, 0, 0);
        MoveEdit(g_edFindId, SX(R_FIND_ID_X1), SY(R_FIND_ID_Y1), SX(R_FIND_ID_X2), SY(R_FIND_ID_Y2), 0, 0, 0, 0);
        MoveEdit(g_edFindResult, SX(R_FIND_RESULT_X1), SY(R_FIND_RESULT_Y1), SX(R_FIND_RESULT_X2), SY(R_FIND_RESULT_Y2), 0, 0, 0, 0);
        return;
    }

    // =========================
    // TEAM_CREATE
    // =========================
    if (g_screen == SCR_TEAM_CREATE) {
        ShowWindow(g_edTcTeamName, SW_SHOW);
        ShowWindow(g_edTcTaskName, SW_SHOW);
        ShowWindow(g_edTcJoinCode, SW_SHOW);

        MoveEdit(g_edTcTeamName, SX(R_TC_TEAM_X1), SY(R_TC_TEAM_Y1), SX(R_TC_TEAM_X2), SY(R_TC_TEAM_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTcTaskName, SX(R_TC_TASK_X1), SY(R_TC_TASK_Y1), SX(R_TC_TASK_X2), SY(R_TC_TASK_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTcJoinCode, SX(R_TC_CODE_X1), SY(R_TC_CODE_Y1), SX(R_TC_CODE_X2), SY(R_TC_CODE_Y2), 0, 0, 0, 0);
        return;
    }

    // =========================
    // TEAM_JOIN
    // =========================
    if (g_screen == SCR_TEAM_JOIN) {
        ShowWindow(g_edTjTeamName, SW_SHOW);
        ShowWindow(g_edTjJoinCode, SW_SHOW);

        MoveEdit(g_edTjTeamName, SX(R_TJ_TEAM_X1), SY(R_TJ_TEAM_Y1), SX(R_TJ_TEAM_X2), SY(R_TJ_TEAM_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTjJoinCode, SX(R_TJ_CODE_X1), SY(R_TJ_CODE_Y1), SX(R_TJ_CODE_X2), SY(R_TJ_CODE_Y2), 0, 0, 0, 0);
        return;
    }

    // =========================
    // MAIN (TeamName/TaskName만 사용)
    // =========================
    if (g_screen == SCR_MAIN) {
        ShowWindow(g_edMainTeamName, SW_SHOW);
        ShowWindow(g_edMainTaskName, SW_SHOW);
        ShowWindow(g_edSearch, SW_SHOW);  // ✅ 추가

        MoveEdit(g_edMainTeamName, SX(R_MAIN_TEAM_X1), SY(R_MAIN_TEAM_Y1), SX(R_MAIN_TEAM_X2), SY(R_MAIN_TEAM_Y2), 0, 0, 0, 0);
        MoveEdit(g_edMainTaskName, SX(R_MAIN_TASK_X1), SY(R_MAIN_TASK_Y1), SX(R_MAIN_TASK_X2), SY(R_MAIN_TASK_Y2), 0, 0, 0, 0);


        return;
    }


    // =========================
    // TASK_ADD
    // =========================
    if (g_screen == SCR_TASK_ADD) {
        ShowWindow(g_edTaItem1, SW_SHOW);
        ShowWindow(g_edTaItem2, SW_SHOW);
        ShowWindow(g_edTaItem3, SW_SHOW);
        ShowWindow(g_edTaItem4, SW_SHOW);
        ShowWindow(g_edTaTitle, SW_SHOW);
        ShowWindow(g_edTaDetail, SW_SHOW);
        ShowWindow(g_edTaFile, SW_SHOW);
        ShowWindow(g_edSearch, SW_SHOW);
        ShowWindow(g_edTaSubDetail, SW_SHOW);


        MoveEdit(g_edTaItem1, SX(R_TA_ITEM1_X1), SY(R_TA_ITEM1_Y1), SX(R_TA_ITEM1_X2), SY(R_TA_ITEM1_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTaItem2, SX(R_TA_ITEM2_X1), SY(R_TA_ITEM2_Y1), SX(R_TA_ITEM2_X2), SY(R_TA_ITEM2_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTaItem3, SX(R_TA_ITEM3_X1), SY(R_TA_ITEM3_Y1), SX(R_TA_ITEM3_X2), SY(R_TA_ITEM3_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTaItem4, SX(R_TA_ITEM4_X1), SY(R_TA_ITEM4_Y1), SX(R_TA_ITEM4_X2), SY(R_TA_ITEM4_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTitle, SX(R_TA_TITLE_X1), SY(R_TA_TITLE_Y1), SX(R_TA_TITLE_X2), SY(R_TA_TITLE_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTaDetail, SX(R_TA_DETAIL_X1), SY(R_TA_DETAIL_Y1), SX(R_TA_DETAIL_X2), SY(R_TA_DETAIL_Y2), 0, 0, 0, 0);
        MoveEdit(g_edTaFile, SX(R_TA_FILE_X1), SY(R_TA_FILE_Y1), SX(R_TA_FILE_X2), SY(R_TA_FILE_Y2), 0, 0, 0, 0);
        MoveEdit(g_edSearch, SX(R_TA_SEARCH_X1), SY(R_TA_SEARCH_Y1),SX(R_TA_SEARCH_X2), SY(R_TA_SEARCH_Y2) , 0, 0, 0, 0);
        MoveEdit(g_edTaSubDetail, SX(R_TA_SUBDETAIL_X1), SY(R_TA_SUBDETAIL_Y1),SX(R_TA_SUBDETAIL_X2), SY(R_TA_SUBDETAIL_Y2), 0, 0, 0, 0);
        return;
    }
    if (g_screen == SCR_BOARD) {
        ShowWindow(g_edBdSearch, SW_SHOW);
        ShowWindow(g_edBdTitle, SW_SHOW);
        ShowWindow(g_edBdContent, SW_SHOW);
        ShowWindow(g_edBdDetail, SW_SHOW);

        MoveEdit(g_edBdSearch,
            SX(R_BD_SEARCH_X1), SY(R_BD_SEARCH_Y1),
            SX(R_BD_SEARCH_X2), SY(R_BD_SEARCH_Y2),
            0, 0, 0, 0
        );

        MoveEdit(g_edBdTitle,
            SX(R_BD_TITLE_X1), SY(R_BD_TITLE_Y1),
            SX(R_BD_TITLE_X2), SY(R_BD_TITLE_Y2),
            0, 0, 0, 0
        );

        MoveEdit(g_edBdContent,
            SX(R_BD_CONTENT_X1), SY(R_BD_CONTENT_Y1),
            SX(R_BD_CONTENT_X2), SY(R_BD_CONTENT_Y2),
            0, 0, 0, 0
        );

        MoveEdit(g_edBdDetail,
            SX(R_BD_DETAIL_X1), SY(R_BD_DETAIL_Y1),
            SX(R_BD_DETAIL_X2), SY(R_BD_DETAIL_Y2),
            0, 0, 0, 0
        );
        return;
    }

}


    static void ResizeToBitmap(HWND hWnd, HBITMAP bmp)
    {
        if (!bmp) return;

        BITMAP bm;
        GetObject(bmp, sizeof(bm), &bm);

        SetWindowClientSize(hWnd, bm.bmWidth, bm.bmHeight);
        g_clientW = bm.bmWidth;
        g_clientH = bm.bmHeight;
 }
    static void SwitchScreen(HWND hWnd, Screen next)
    {
        DestroyAllEdits();
        g_screen = next;

        if (next == SCR_START)             ResizeToBitmap(hWnd, g_bmpStart);
        else if (next == SCR_SIGNUP)       ResizeToBitmap(hWnd, g_bmpSignup);
        else if (next == SCR_FINDPW)       ResizeToBitmap(hWnd, g_bmpFindPw);
        else if (next == SCR_DEADLINE)     ResizeToBitmap(hWnd, g_bmpDeadline);
        else if (next == SCR_TODO)         ResizeToBitmap(hWnd, g_bmpTodo);
        else if (next == SCR_MYTEAM)       ResizeToBitmap(hWnd, g_bmpMyTeam);
        else if (next == SCR_DONE)         ResizeToBitmap(hWnd, g_bmpDone);
        else if (next == SCR_TEAM_CREATE)  ResizeToBitmap(hWnd, g_bmpTeamCreate);
        else if (next == SCR_TEAM_JOIN)    ResizeToBitmap(hWnd, g_bmpTeamJoin);
        else if (next == SCR_TASK_ADD)     ResizeToBitmap(hWnd, g_bmpTaskAdd);
        else if (next == SCR_BOARD)        ResizeToBitmap(hWnd, g_bmpBoard);
        else                               ResizeToBitmap(hWnd, g_bmpMain);

        UpdateTopLoginRect();  // ✅ 여기 추가 (ResizeToBitmap 이후!)

        CreateControlsForScreen(hWnd, next);
        RelayoutControls(hWnd);

        InvalidateRect(hWnd, NULL, FALSE);
    }

static void DrawDebugOverlay(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);

    HFONT f = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT old = (HFONT)SelectObject(hdc, f);

    wchar_t buf[128];
    swprintf(buf, 128, L"Last Click: (%d, %d)  Client:(%d,%d)", g_lastX, g_lastY, g_clientW, g_clientH);
    TextOutW(hdc, 20, 20, buf, (int)wcslen(buf));

    SelectObject(hdc, old);
    DeleteObject(f);
}

// =========================================================
// 라이프사이클
// =========================================================
int App_OnCreate(HWND hWnd)
{
    int w = 0, h = 0;

    g_bmpStart = LoadBmpFromExeDir(hWnd, BMP_START, &w, &h);
    if (!g_bmpStart) return -1;

    g_bmpSignup = LoadBmpFromExeDir(hWnd, BMP_SIGNUP, NULL, NULL);
    g_bmpMain = LoadBmpFromExeDir(hWnd, BMP_MAIN, NULL, NULL);
    g_bmpFindPw = LoadBmpFromExeDir(hWnd, BMP_FINDPW, NULL, NULL);
    g_bmpDeadline = LoadBmpFromExeDir(hWnd, BMP_DEADLINE, NULL, NULL);
    g_bmpTodo = LoadBmpFromExeDir(hWnd, BMP_TODO, NULL, NULL);
    g_bmpMyTeam = LoadBmpFromExeDir(hWnd, BMP_MYTEAM, NULL, NULL);
    g_bmpDone = LoadBmpFromExeDir(hWnd, BMP_DONE, NULL, NULL);
    g_bmpTeamCreate = LoadBmpFromExeDir(hWnd, BMP_TEAM_CREATE, NULL, NULL);
    g_bmpTeamJoin = LoadBmpFromExeDir(hWnd, BMP_TEAM_JOIN, NULL, NULL);
    g_bmpTaskAdd = LoadBmpFromExeDir(hWnd, BMP_TASK_ADD, NULL, NULL);
    g_bmpBoard = LoadBmpFromExeDir(hWnd, BMP_BOARD, NULL, NULL);

    if (!g_bmpSignup || !g_bmpMain || !g_bmpFindPw || !g_bmpDeadline || !g_bmpTodo ||
        !g_bmpMyTeam || !g_bmpDone || !g_bmpTeamCreate || !g_bmpTeamJoin ||
        !g_bmpTaskAdd || !g_bmpBoard) return -1;

    SwitchScreen(hWnd, SCR_START);
    return 0;
}

void App_OnSize(HWND hWnd, int w, int h)
{
    g_clientW = w;
    g_clientH = h;

    UpdateTopLoginRect();

    RelayoutControls(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

void App_OnLButtonDown(HWND hWnd, int x, int y)
{
    g_lastX = x;
    g_lastY = y;
    InvalidateRect(hWnd, NULL, FALSE);

    // START 영역
    int s_login_x1 = SX(R_BTN_LOGIN_X1), s_login_y1 = SY(R_BTN_LOGIN_Y1);
    int s_login_x2 = SX(R_BTN_LOGIN_X2), s_login_y2 = SY(R_BTN_LOGIN_Y2);

    int s_to_signup_x1 = SX(R_BTN_TO_SIGNUP_X1), s_to_signup_y1 = SY(R_BTN_TO_SIGNUP_Y1);
    int s_to_signup_x2 = SX(R_BTN_TO_SIGNUP_X2), s_to_signup_y2 = SY(R_BTN_TO_SIGNUP_Y2);

    int s_find_x1 = SX(R_BTN_FINDPW_X1), s_find_y1 = SY(R_BTN_FINDPW_Y1);
    int s_find_x2 = SX(R_BTN_FINDPW_X2), s_find_y2 = SY(R_BTN_FINDPW_Y2);

    // SIGNUP
    int b_back_x1 = SX(R_BTN_BACK_X1), b_back_y1 = SY(R_BTN_BACK_Y1);
    int b_back_x2 = SX(R_BTN_BACK_X2), b_back_y2 = SY(R_BTN_BACK_Y2);

    int b_cr_x1 = SX(R_BTN_CREATE_X1), b_cr_y1 = SY(R_BTN_CREATE_Y1);
    int b_cr_x2 = SX(R_BTN_CREATE_X2), b_cr_y2 = SY(R_BTN_CREATE_Y2);

    // MAIN 오른쪽 메뉴(모든 화면에서 공통으로 쓰기)
    int r_tc_x1 = SX(R_MAIN_BTN_TEAM_CREATE_X1), r_tc_y1 = SY(R_MAIN_BTN_TEAM_CREATE_Y1);
    int r_tc_x2 = SX(R_MAIN_BTN_TEAM_CREATE_X2), r_tc_y2 = SY(R_MAIN_BTN_TEAM_CREATE_Y2);

    int r_tj_x1 = SX(R_MAIN_BTN_TEAM_JOIN_X1), r_tj_y1 = SY(R_MAIN_BTN_TEAM_JOIN_Y1);
    int r_tj_x2 = SX(R_MAIN_BTN_TEAM_JOIN_X2), r_tj_y2 = SY(R_MAIN_BTN_TEAM_JOIN_Y2);

    int r_ta_x1 = SX(R_MAIN_BTN_TASK_ADD_X1), r_ta_y1 = SY(R_MAIN_BTN_TASK_ADD_Y1);
    int r_ta_x2 = SX(R_MAIN_BTN_TASK_ADD_X2), r_ta_y2 = SY(R_MAIN_BTN_TASK_ADD_Y2);

    int r_bd_x1 = SX(R_MAIN_BTN_BOARD_X1), r_bd_y1 = SY(R_MAIN_BTN_BOARD_Y1);
    int r_bd_x2 = SX(R_MAIN_BTN_BOARD_X2), r_bd_y2 = SY(R_MAIN_BTN_BOARD_Y2);

    // MAIN 왼쪽
    int m_dead_x1 = SX(R_MAIN_BTN_DEADLINE_X1), m_dead_y1 = SY(R_MAIN_BTN_DEADLINE_Y1);
    int m_dead_x2 = SX(R_MAIN_BTN_DEADLINE_X2), m_dead_y2 = SY(R_MAIN_BTN_DEADLINE_Y2);

    int m_todo_x1 = SX(R_MAIN_BTN_TODO_X1), m_todo_y1 = SY(R_MAIN_BTN_TODO_Y1);
    int m_todo_x2 = SX(R_MAIN_BTN_TODO_X2), m_todo_y2 = SY(R_MAIN_BTN_TODO_Y2);

    int m_my_x1 = SX(R_MAIN_BTN_MYTEAM_X1), m_my_y1 = SY(R_MAIN_BTN_MYTEAM_Y1);
    int m_my_x2 = SX(R_MAIN_BTN_MYTEAM_X2), m_my_y2 = SY(R_MAIN_BTN_MYTEAM_Y2);

    int m_done_x1 = SX(R_MAIN_BTN_DONE_X1), m_done_y1 = SY(R_MAIN_BTN_DONE_Y1);
    int m_done_x2 = SX(R_MAIN_BTN_DONE_X2), m_done_y2 = SY(R_MAIN_BTN_DONE_Y2);

    // =========================================================
    // 0) 폼 화면(팀등록/팀참여/과제등록/게시판)일 때:
    //    - 패널 바깥 클릭 -> MAIN 복귀
    //    - 오른쪽 메뉴는 계속 동작
    // =========================================================
    {
        RECT rcPanel;
        if (GetPanelRectForScreen(g_screen, &rcPanel)) {

            // ✅ 오른쪽 메뉴는 어디서든 클릭 가능
            if (PtInRectXY(x, y, r_tc_x1, r_tc_y1, r_tc_x2, r_tc_y2)) { SwitchScreen(hWnd, SCR_TEAM_CREATE); return; }
            if (PtInRectXY(x, y, r_tj_x1, r_tj_y1, r_tj_x2, r_tj_y2)) { SwitchScreen(hWnd, SCR_TEAM_JOIN); return; }
            if (PtInRectXY(x, y, r_ta_x1, r_ta_y1, r_ta_x2, r_ta_y2)) { SwitchScreen(hWnd, SCR_TASK_ADD); return; }
            if (PtInRectXY(x, y, r_bd_x1, r_bd_y1, r_bd_x2, r_bd_y2)) { SwitchScreen(hWnd, SCR_BOARD); return; }

            POINT pt = { x, y };
            if (!PtInRect(&rcPanel, pt)) {
                SwitchScreen(hWnd, SCR_MAIN);
                return;
            }
           
        }
    }

    // =========================================================
    // 1) START
    // =========================================================
    if (g_screen == SCR_START) {
        if (PtInRectXY(x, y, SX(R_START_ID_X1), SY(R_START_ID_Y1), SX(R_START_ID_X2), SY(R_START_ID_Y2))) { SetFocus(g_edStartId); return; }
        if (PtInRectXY(x, y, SX(R_START_PW_X1), SY(R_START_PW_Y1), SX(R_START_PW_X2), SY(R_START_PW_Y2))) { SetFocus(g_edStartPw); return; }

        if (PtInRectXY(x, y, s_find_x1, s_find_y1, s_find_x2, s_find_y2)) { SwitchScreen(hWnd, SCR_FINDPW); return; }
        if (PtInRectXY(x, y, s_to_signup_x1, s_to_signup_y1, s_to_signup_x2, s_to_signup_y2)) { SwitchScreen(hWnd, SCR_SIGNUP); return; }
        if (PtInRectXY(x, y, s_login_x1, s_login_y1, s_login_x2, s_login_y2)) { SwitchScreen(hWnd, SCR_MAIN); return; }
        return;
    }

    // =========================================================
    // 2) SIGNUP
    // =========================================================
    if (g_screen == SCR_SIGNUP) {
        POINT pt = { x, y };
        if (PtInRect(&g_rcTopLogin, pt)) { SwitchScreen(hWnd, SCR_START); return; }

        if (PtInRectXY(x, y, b_back_x1, b_back_y1, b_back_x2, b_back_y2)) { SwitchScreen(hWnd, SCR_START); return; }
        if (PtInRectXY(x, y, b_cr_x1, b_cr_y1, b_cr_x2, b_cr_y2)) { SwitchScreen(hWnd, SCR_MAIN); return; }
        return;
    }

    // =========================================================
    // 3) FINDPW
    // =========================================================
    if (g_screen == SCR_FINDPW) {
        POINT pt = { x, y };
        if (PtInRect(&g_rcTopLogin, pt)) { SwitchScreen(hWnd, SCR_START); return; }

        if (PtInRectXY(x, y, SX(R_FIND_NAME_X1), SY(R_FIND_NAME_Y1), SX(R_FIND_NAME_X2), SY(R_FIND_NAME_Y2))) { SetFocus(g_edFindName); return; }
        if (PtInRectXY(x, y, SX(R_FIND_ID_X1), SY(R_FIND_ID_Y1), SX(R_FIND_ID_X2), SY(R_FIND_ID_Y2))) { SetFocus(g_edFindId); return; }
        return;
    }

    // =========================================================
    // 4) MAIN
    // =========================================================
    if (g_screen == SCR_MAIN) {

        // ✅ 마감 오버레이 떠있을 때: X / 바깥 클릭으로 닫기
        if (g_overlay == OVR_DEADLINE) {
            POINT pt = { x, y };

            if (PtInRect(&g_rcDeadlineClose, pt)) {
                g_overlay = OVR_NONE;
                InvalidateRect(hWnd, NULL, FALSE);
                return;
            }

            if (!PtInRect(&g_rcDeadlinePanel, pt)) {
                g_overlay = OVR_NONE;
                InvalidateRect(hWnd, NULL, FALSE);
                return;
            }

            // 패널 내부 클릭은 일단 무시
            return;
        }

        // ✅ 왼쪽 버튼들
        if (PtInRectXY(x, y, m_dead_x1, m_dead_y1, m_dead_x2, m_dead_y2)) { SwitchScreen(hWnd, SCR_DEADLINE); return; }
        if (PtInRectXY(x, y, m_todo_x1, m_todo_y1, m_todo_x2, m_todo_y2)) { SwitchScreen(hWnd, SCR_TODO); return; }
        if (PtInRectXY(x, y, m_my_x1, m_my_y1, m_my_x2, m_my_y2)) { SwitchScreen(hWnd, SCR_MYTEAM); return; }
        if (PtInRectXY(x, y, m_done_x1, m_done_y1, m_done_x2, m_done_y2)) { SwitchScreen(hWnd, SCR_DONE); return; }

        // ✅ 오른쪽 메뉴
        if (PtInRectXY(x, y, r_tc_x1, r_tc_y1, r_tc_x2, r_tc_y2)) { SwitchScreen(hWnd, SCR_TEAM_CREATE); return; }
        if (PtInRectXY(x, y, r_tj_x1, r_tj_y1, r_tj_x2, r_tj_y2)) { SwitchScreen(hWnd, SCR_TEAM_JOIN); return; }
        if (PtInRectXY(x, y, r_ta_x1, r_ta_y1, r_ta_x2, r_ta_y2)) { SwitchScreen(hWnd, SCR_TASK_ADD); return; }
        if (PtInRectXY(x, y, r_bd_x1, r_bd_y1, r_bd_x2, r_bd_y2)) { SwitchScreen(hWnd, SCR_BOARD); return; }

        return;
    }

    // 나머지 화면들은 일단 아무 동작 없음
}







void App_OnPaint(HWND hWnd, HDC hdc)
{
    RECT rc;
    GetClientRect(hWnd, &rc);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    g_clientW = w;
    g_clientH = h;

    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP back = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBack = (HBITMAP)SelectObject(mem, back);

    FillRect(mem, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

    if (g_screen == SCR_START)            DrawBitmapFit(mem, g_bmpStart, w, h);
    else if (g_screen == SCR_SIGNUP)      DrawBitmapFit(mem, g_bmpSignup, w, h);
    else if (g_screen == SCR_FINDPW)      DrawBitmapFit(mem, g_bmpFindPw, w, h);
    else if (g_screen == SCR_DEADLINE)    DrawBitmapFit(mem, g_bmpDeadline, w, h);
    else if (g_screen == SCR_TODO)        DrawBitmapFit(mem, g_bmpTodo, w, h);
    else if (g_screen == SCR_MYTEAM)      DrawBitmapFit(mem, g_bmpMyTeam, w, h);
    else if (g_screen == SCR_DONE)        DrawBitmapFit(mem, g_bmpDone, w, h);
    else if (g_screen == SCR_TEAM_CREATE) DrawBitmapFit(mem, g_bmpTeamCreate, w, h);
    else if (g_screen == SCR_TEAM_JOIN)   DrawBitmapFit(mem, g_bmpTeamJoin, w, h);
    else if (g_screen == SCR_TASK_ADD)    DrawBitmapFit(mem, g_bmpTaskAdd, w, h);
    else if (g_screen == SCR_BOARD)       DrawBitmapFit(mem, g_bmpBoard, w, h);
    else                                  DrawBitmapFit(mem, g_bmpMain, w, h);

    DrawDebugOverlay(mem);

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBack);
    DeleteObject(back);
    DeleteDC(mem);
    (void)hWnd;
}

void App_OnDestroy(void)
{
    DestroyAllEdits();

    if (g_bmpStart) { DeleteObject(g_bmpStart);      g_bmpStart = NULL; }
    if (g_bmpSignup) { DeleteObject(g_bmpSignup);     g_bmpSignup = NULL; }
    if (g_bmpMain) { DeleteObject(g_bmpMain);       g_bmpMain = NULL; }
    if (g_bmpFindPw) { DeleteObject(g_bmpFindPw);     g_bmpFindPw = NULL; }
    if (g_edFindName) { RemovePropW(g_edFindName, PROP_OLDPROC); DestroyWindow(g_edFindName);   g_edFindName = NULL; }
    if (g_edFindId) { RemovePropW(g_edFindId, PROP_OLDPROC); DestroyWindow(g_edFindId);     g_edFindId = NULL; }
    if (g_edFindResult) { RemovePropW(g_edFindResult, PROP_OLDPROC); DestroyWindow(g_edFindResult); g_edFindResult = NULL; }
    if (g_bmpDeadline) { DeleteObject(g_bmpDeadline);   g_bmpDeadline = NULL; }
    if (g_bmpTodo) { DeleteObject(g_bmpTodo);       g_bmpTodo = NULL; }
    if (g_bmpMyTeam) { DeleteObject(g_bmpMyTeam);     g_bmpMyTeam = NULL; }
    if (g_bmpDone) { DeleteObject(g_bmpDone);       g_bmpDone = NULL; }
    if (g_bmpTeamCreate) { DeleteObject(g_bmpTeamCreate); g_bmpTeamCreate = NULL; }
    if (g_bmpTeamJoin) { DeleteObject(g_bmpTeamJoin);   g_bmpTeamJoin = NULL; }
    if (g_bmpTaskAdd) { DeleteObject(g_bmpTaskAdd);    g_bmpTaskAdd = NULL; }
    if (g_bmpBoard) { DeleteObject(g_bmpBoard);      g_bmpBoard = NULL; }
    if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = NULL; }
    if (g_edTaItem1) { RemovePropW(g_edTaItem1, PROP_OLDPROC); DestroyWindow(g_edTaItem1);  g_edTaItem1 = NULL; }
    if (g_edTaItem2) { RemovePropW(g_edTaItem2, PROP_OLDPROC); DestroyWindow(g_edTaItem2);  g_edTaItem2 = NULL; }
    if (g_edTaItem3) { RemovePropW(g_edTaItem3, PROP_OLDPROC); DestroyWindow(g_edTaItem3);  g_edTaItem3 = NULL; }
    if (g_edTaItem4) { RemovePropW(g_edTaItem4, PROP_OLDPROC); DestroyWindow(g_edTaItem4);  g_edTaItem4 = NULL; }

    if (g_edTaTitle) { RemovePropW(g_edTaTitle, PROP_OLDPROC); DestroyWindow(g_edTaTitle);  g_edTaTitle = NULL; }
    if (g_edTaDetail) { RemovePropW(g_edTaDetail, PROP_OLDPROC); DestroyWindow(g_edTaDetail); g_edTaDetail = NULL; }
    if (g_edTaFile) { RemovePropW(g_edTaFile, PROP_OLDPROC); DestroyWindow(g_edTaFile);   g_edTaFile = NULL; }

}



void App_GoToStart(HWND hWnd)
{
    SwitchScreen(hWnd, SCR_START);
}


