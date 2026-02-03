// =========================================================
// app.c  (구조 고정/정리 버전) - 복붙용
// - 공통 뒤로가기(투명 + UI버튼)
// - 팀등록 완료 -> 메인 상단 팀명/과제명 반영
// - 내팀 토글 + 리스트 더블클릭 팀 전환
// - TASK_ADD 텍스트필드 복구
// - BOARD는 board.c에서 컨트롤 생성/배치/클릭 처리 (app.c는 라우팅만)
//
// ✅ 앞으로 기능을 추가해도 "건들지 말아야 할" 핵심 구역은
//    [DO NOT TOUCH] 주석으로 표시해둠.
// =========================================================

#define _CRT_SECURE_NO_WARNINGS

#include "app.h"
#include "ui_coords.h"
#include "ui_layout.h"
#include "bmp_loader.h"
#include "auth.h"
#include "board.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <windowsx.h>
#include <string.h>   // ✅ memmove

// =========================================================
// [DO NOT TOUCH] 좌표/히트테스트 유틸
// =========================================================
static RECT MakeRcScaled(int x1, int y1, int x2, int y2)
{
    int L = SX(x1), T = SY(y1), R = SX(x2), B = SY(y2);

    RECT rc;
    rc.left = (L < R) ? L : R;
    rc.top = (T < B) ? T : B;
    rc.right = (L < R) ? R : L;
    rc.bottom = (T < B) ? B : T;
    return rc;
}

static int HitScaled(int x1, int y1, int x2, int y2, int x, int y)
{
    RECT rc = MakeRcScaled(x1, y1, x2, y2);
    POINT pt = { x, y };
    return PtInRect(&rc, pt);
}

// =========================================================
// Forward decl
// =========================================================
static void RelayoutControls(HWND hWnd);
static void ResizeToBitmap(HWND hWnd, HBITMAP bmp);
static void SwitchScreen(HWND hWnd, Screen next);
static void SwitchScreen_NoHistory(HWND hWnd, Screen next);
static void DrawDebugOverlay(HDC hdc);
static HFONT GetUIFont(void);

void RefreshMyTeamList(HWND hWnd);
void SwitchToTeam(HWND hWnd, const wchar_t* teamId);

// =========================================================
// 전역 상태
// =========================================================
Screen g_screen = SCR_START;

typedef enum { OVR_NONE = 0, OVR_DEADLINE = 1 } Overlay;
static Overlay g_overlay = OVR_NONE;

// (너가 오버레이 더 만들면 여기 늘려도 됨)
static RECT g_rcDeadlinePanel = { 0,0,0,0 };
static RECT g_rcDeadlineClose = { 0,0,0,0 };

// BMP 파일명
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

// BMP 핸들
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

// 디버그/클라이언트 크기
int g_lastX = -1, g_lastY = -1;
int g_clientW = 0, g_clientH = 0;



// 로그인/팀 상태
wchar_t g_currentUserId[128] = L"";
wchar_t g_currentTeamId[64] = L"";

// =========================================================
// [DO NOT TOUCH] MAIN 상단 팀명/과제명 유지용 전역
// =========================================================
static wchar_t g_mainTeamText[128] = L"";
static wchar_t g_mainTaskText[128] = L"";

// =========================================================
// [DO NOT TOUCH] 화면 히스토리(뒤로가기)
// =========================================================
#define NAV_STACK_MAX 32
static Screen g_navStack[NAV_STACK_MAX];
static int    g_navTop = 0;

static void NavPush(Screen s)
{
    if (g_navTop < NAV_STACK_MAX) {
        g_navStack[g_navTop++] = s;
    }
    else {
        memmove(&g_navStack[0], &g_navStack[1], sizeof(Screen) * (NAV_STACK_MAX - 1));
        g_navStack[NAV_STACK_MAX - 1] = s;
        g_navTop = NAV_STACK_MAX;
    }
}

static int NavCanGoBack(void) { return (g_navTop > 0); }

static Screen NavPop(void)
{
    if (g_navTop <= 0) return SCR_START;
    return g_navStack[--g_navTop];
}

static void GoBack(HWND hWnd)
{
    if (!NavCanGoBack()) return;
    SwitchScreen_NoHistory(hWnd, NavPop());
}

// =========================================================
// 폰트
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
// [DO NOT TOUCH] Edit 클릭도 Last Click 잡히게 하는 훅
// =========================================================
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

// ✅ 외부(board.c)에서도 쓰기 위해 공개 함수로 변경
HWND App_CreateEdit(HWND parent, int ctrlId, DWORD extraStyle)
{
    DWORD style = WS_CHILD | WS_VISIBLE | ES_LEFT | extraStyle;
    if ((extraStyle & ES_MULTILINE) == 0) style |= ES_AUTOHSCROLL;

    HWND h = CreateWindowExW(
        0, L"EDIT", L"",
        style,
        10, 10, 100, 30,
        parent, (HMENU)(INT_PTR)ctrlId,
        GetModuleHandleW(NULL), NULL
    );

    HFONT f = GetUIFont();
    if (f) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);

    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)EditHookProc);
    SetPropW(h, PROP_OLDPROC, (HANDLE)oldProc);

    return h;
}

// ✅ app.c 내부 기존 코드 변경 최소화(기존 CreateEdit(...) 호출 그대로 유지)
#define CreateEdit App_CreateEdit

// =========================================================
// Edit 컨트롤들
// =========================================================
static HWND g_edStartId = NULL;
static HWND g_edStartPw = NULL;

static HWND g_edSignName = NULL;
static HWND g_edSignId = NULL;
static HWND g_edSignPw = NULL;

static HWND g_edFindName = NULL;
static HWND g_edFindId = NULL;
static HWND g_edFindResult = NULL;

static HWND g_edMainTeamName = NULL;
static HWND g_edMainTaskName = NULL;
static HWND g_edSearch = NULL;

static HWND g_edTcTeam = NULL;
static HWND g_edTcTask = NULL;
static HWND g_edTcCode = NULL;

// TEAM_JOIN
static HWND g_edTjTeam = NULL;
static HWND g_edTjCode = NULL;

// TASK_ADD (복구)
static HWND g_edTaSearch = NULL;
static HWND g_edTaTask1 = NULL;
static HWND g_edTaTask2 = NULL;
static HWND g_edTaTask3 = NULL;
static HWND g_edTaTask4 = NULL;
static HWND g_edTaTitle = NULL;
static HWND g_edTaContent = NULL;  // 멀티라인
static HWND g_edTaDetail = NULL;   // 단일라인
static HWND g_edTaFile = NULL;     // 단일라인

// =========================================================
// 내팀 ListBox (1번만 생성)
// =========================================================
#define ID_LB_MYTEAMS  4100
static HWND g_lbMyTeams = NULL;
static int  g_showMyTeams = 0;

// ---------------------------------------------------------
// [DO NOT TOUCH] 모든 Edit 제거
// ---------------------------------------------------------
static void DestroyAllEdits(void)
{
    if (g_edStartId) { RemovePropW(g_edStartId, PROP_OLDPROC); DestroyWindow(g_edStartId); g_edStartId = NULL; }
    if (g_edStartPw) { RemovePropW(g_edStartPw, PROP_OLDPROC); DestroyWindow(g_edStartPw); g_edStartPw = NULL; }

    if (g_edSignName) { RemovePropW(g_edSignName, PROP_OLDPROC); DestroyWindow(g_edSignName); g_edSignName = NULL; }
    if (g_edSignId) { RemovePropW(g_edSignId, PROP_OLDPROC); DestroyWindow(g_edSignId); g_edSignId = NULL; }
    if (g_edSignPw) { RemovePropW(g_edSignPw, PROP_OLDPROC); DestroyWindow(g_edSignPw); g_edSignPw = NULL; }

    if (g_edFindName) { RemovePropW(g_edFindName, PROP_OLDPROC); DestroyWindow(g_edFindName); g_edFindName = NULL; }
    if (g_edFindId) { RemovePropW(g_edFindId, PROP_OLDPROC); DestroyWindow(g_edFindId); g_edFindId = NULL; }
    if (g_edFindResult) { RemovePropW(g_edFindResult, PROP_OLDPROC); DestroyWindow(g_edFindResult); g_edFindResult = NULL; }

    if (g_edMainTeamName) { RemovePropW(g_edMainTeamName, PROP_OLDPROC); DestroyWindow(g_edMainTeamName); g_edMainTeamName = NULL; }
    if (g_edMainTaskName) { RemovePropW(g_edMainTaskName, PROP_OLDPROC); DestroyWindow(g_edMainTaskName); g_edMainTaskName = NULL; }
    if (g_edSearch) { RemovePropW(g_edSearch, PROP_OLDPROC); DestroyWindow(g_edSearch); g_edSearch = NULL; }

    if (g_edTcTeam) { RemovePropW(g_edTcTeam, PROP_OLDPROC); DestroyWindow(g_edTcTeam); g_edTcTeam = NULL; }
    if (g_edTcTask) { RemovePropW(g_edTcTask, PROP_OLDPROC); DestroyWindow(g_edTcTask); g_edTcTask = NULL; }
    if (g_edTcCode) { RemovePropW(g_edTcCode, PROP_OLDPROC); DestroyWindow(g_edTcCode); g_edTcCode = NULL; }

    if (g_edTjTeam) { RemovePropW(g_edTjTeam, PROP_OLDPROC); DestroyWindow(g_edTjTeam); g_edTjTeam = NULL; }
    if (g_edTjCode) { RemovePropW(g_edTjCode, PROP_OLDPROC); DestroyWindow(g_edTjCode); g_edTjCode = NULL; }

    // TASK_ADD
    if (g_edTaSearch) { RemovePropW(g_edTaSearch, PROP_OLDPROC); DestroyWindow(g_edTaSearch); g_edTaSearch = NULL; }
    if (g_edTaTask1) { RemovePropW(g_edTaTask1, PROP_OLDPROC); DestroyWindow(g_edTaTask1); g_edTaTask1 = NULL; }
    if (g_edTaTask2) { RemovePropW(g_edTaTask2, PROP_OLDPROC); DestroyWindow(g_edTaTask2); g_edTaTask2 = NULL; }
    if (g_edTaTask3) { RemovePropW(g_edTaTask3, PROP_OLDPROC); DestroyWindow(g_edTaTask3); g_edTaTask3 = NULL; }
    if (g_edTaTask4) { RemovePropW(g_edTaTask4, PROP_OLDPROC); DestroyWindow(g_edTaTask4); g_edTaTask4 = NULL; }
    if (g_edTaTitle) { RemovePropW(g_edTaTitle, PROP_OLDPROC); DestroyWindow(g_edTaTitle); g_edTaTitle = NULL; }
    if (g_edTaContent) { RemovePropW(g_edTaContent, PROP_OLDPROC); DestroyWindow(g_edTaContent); g_edTaContent = NULL; }
    if (g_edTaDetail) { RemovePropW(g_edTaDetail, PROP_OLDPROC); DestroyWindow(g_edTaDetail); g_edTaDetail = NULL; }
    if (g_edTaFile) { RemovePropW(g_edTaFile, PROP_OLDPROC); DestroyWindow(g_edTaFile); g_edTaFile = NULL; }

    // ✅ BOARD 컨트롤은 board.c가 관리
    Board_DestroyControls();
}

// =========================================================
// [DO NOT TOUCH] 화면별 컨트롤 생성
// =========================================================
static void CreateControlsForScreen(HWND hWnd, Screen s)
{
    DestroyAllEdits();

    switch (s)
    {
    case SCR_START:
        g_edStartId = CreateEdit(hWnd, 101, 0);
        g_edStartPw = CreateEdit(hWnd, 102, ES_PASSWORD);
        break;

    case SCR_SIGNUP:
        g_edSignName = CreateEdit(hWnd, 201, 0);
        g_edSignId = CreateEdit(hWnd, 202, 0);
        g_edSignPw = CreateEdit(hWnd, 203, ES_PASSWORD);
        break;

    case SCR_FINDPW:
        g_edFindName = CreateEdit(hWnd, 301, 0);
        g_edFindId = CreateEdit(hWnd, 302, 0);
        g_edFindResult = CreateEdit(hWnd, 303, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
        break;

    case SCR_MAIN:
        g_edMainTeamName = CreateEdit(hWnd, 601, 0);
        g_edMainTaskName = CreateEdit(hWnd, 602, 0);
        g_edSearch = CreateEdit(hWnd, 603, 0);

        if (g_edMainTeamName) SetWindowTextW(g_edMainTeamName, g_mainTeamText);
        if (g_edMainTaskName) SetWindowTextW(g_edMainTaskName, g_mainTaskText);
        break;

    case SCR_TEAM_CREATE:
        g_edTcTeam = CreateEdit(hWnd, 701, 0);
        g_edTcTask = CreateEdit(hWnd, 702, 0);
        g_edTcCode = CreateEdit(hWnd, 703, 0);
        break;

    case SCR_TEAM_JOIN:
        g_edTjTeam = CreateEdit(hWnd, 801, 0);
        g_edTjCode = CreateEdit(hWnd, 802, 0);
        break;

    case SCR_TASK_ADD:
        g_edTaSearch = CreateEdit(hWnd, 901, 0);

        g_edTaTask1 = CreateEdit(hWnd, 902, 0);
        g_edTaTask2 = CreateEdit(hWnd, 903, 0);
        g_edTaTask3 = CreateEdit(hWnd, 904, 0);
        g_edTaTask4 = CreateEdit(hWnd, 905, 0);

        g_edTaTitle = CreateEdit(hWnd, 906, 0);
        g_edTaContent = CreateEdit(hWnd, 907, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);

        g_edTaDetail = CreateEdit(hWnd, 908, 0);
        g_edTaFile = CreateEdit(hWnd, 909, 0);
        break;

    case SCR_BOARD:
        Board_CreateControls(hWnd);
        break;

    default:
        break;
    }

    // ✅ 화면 들어가자마자 바로 배치
    RelayoutControls(hWnd);
}

// =========================================================
// [DO NOT TOUCH] 레이아웃
// =========================================================
static void RelayoutControls(HWND hWnd)
{
    (void)hWnd;

    // 모두 숨김
    if (g_edStartId) ShowWindow(g_edStartId, SW_HIDE);
    if (g_edStartPw) ShowWindow(g_edStartPw, SW_HIDE);

    if (g_edSignName) ShowWindow(g_edSignName, SW_HIDE);
    if (g_edSignId) ShowWindow(g_edSignId, SW_HIDE);
    if (g_edSignPw) ShowWindow(g_edSignPw, SW_HIDE);

    if (g_edFindName) ShowWindow(g_edFindName, SW_HIDE);
    if (g_edFindId) ShowWindow(g_edFindId, SW_HIDE);
    if (g_edFindResult) ShowWindow(g_edFindResult, SW_HIDE);

    if (g_edMainTeamName) ShowWindow(g_edMainTeamName, SW_HIDE);
    if (g_edMainTaskName) ShowWindow(g_edMainTaskName, SW_HIDE);
    if (g_edSearch) ShowWindow(g_edSearch, SW_HIDE);

    if (g_edTcTeam) ShowWindow(g_edTcTeam, SW_HIDE);
    if (g_edTcTask) ShowWindow(g_edTcTask, SW_HIDE);
    if (g_edTcCode) ShowWindow(g_edTcCode, SW_HIDE);

    if (g_edTjTeam) ShowWindow(g_edTjTeam, SW_HIDE);
    if (g_edTjCode) ShowWindow(g_edTjCode, SW_HIDE);

    if (g_lbMyTeams) ShowWindow(g_lbMyTeams, SW_HIDE);

    // TASK_ADD 숨김
    if (g_edTaSearch) ShowWindow(g_edTaSearch, SW_HIDE);
    if (g_edTaTask1) ShowWindow(g_edTaTask1, SW_HIDE);
    if (g_edTaTask2) ShowWindow(g_edTaTask2, SW_HIDE);
    if (g_edTaTask3) ShowWindow(g_edTaTask3, SW_HIDE);
    if (g_edTaTask4) ShowWindow(g_edTaTask4, SW_HIDE);
    if (g_edTaTitle) ShowWindow(g_edTaTitle, SW_HIDE);
    if (g_edTaContent) ShowWindow(g_edTaContent, SW_HIDE);
    if (g_edTaDetail) ShowWindow(g_edTaDetail, SW_HIDE);
    if (g_edTaFile) ShowWindow(g_edTaFile, SW_HIDE);

    // START
    if (g_screen == SCR_START) {
        ShowWindow(g_edStartId, SW_SHOW);
        ShowWindow(g_edStartPw, SW_SHOW);

        MoveEdit(g_edStartId, SX(R_START_ID_X1), SY(R_START_ID_Y1),
            SX(R_START_ID_X2), SY(R_START_ID_Y2), 0, 0, 0, 0);

        MoveEdit(g_edStartPw, SX(R_START_PW_X1), SY(R_START_PW_Y1),
            SX(R_START_PW_X2), SY(R_START_PW_Y2), 0, 0, 0, 0);
        return;
    }

    // SIGNUP
    if (g_screen == SCR_SIGNUP) {
        ShowWindow(g_edSignName, SW_SHOW);
        ShowWindow(g_edSignId, SW_SHOW);
        ShowWindow(g_edSignPw, SW_SHOW);

        MoveEdit(g_edSignName, SX(R_SIGN_NAME_X1), SY(R_SIGN_NAME_Y1),
            SX(R_SIGN_NAME_X2), SY(R_SIGN_NAME_Y2), 0, 0, 0, 0);

        MoveEdit(g_edSignId, SX(R_SIGN_ID_X1), SY(R_SIGN_ID_Y1),
            SX(R_SIGN_ID_X2), SY(R_SIGN_ID_Y2), 0, 0, 0, 0);

        MoveEdit(g_edSignPw, SX(R_SIGN_PW_X1), SY(R_SIGN_PW_Y1),
            SX(R_SIGN_PW_X2), SY(R_SIGN_PW_Y2), 0, 0, 0, 0);
        return;
    }

    // FINDPW
    if (g_screen == SCR_FINDPW) {
        ShowWindow(g_edFindName, SW_SHOW);
        ShowWindow(g_edFindId, SW_SHOW);
        ShowWindow(g_edFindResult, SW_SHOW);

        MoveEdit(g_edFindName, SX(R_FIND_NAME_X1), SY(R_FIND_NAME_Y1),
            SX(R_FIND_NAME_X2), SY(R_FIND_NAME_Y2), 0, 0, 0, 0);

        MoveEdit(g_edFindId, SX(R_FIND_ID_X1), SY(R_FIND_ID_Y1),
            SX(R_FIND_ID_X2), SY(R_FIND_ID_Y2), 0, 0, 0, 0);

        MoveEdit(g_edFindResult, SX(R_FIND_RESULT_X1), SY(R_FIND_RESULT_Y1),
            SX(R_FIND_RESULT_X2), SY(R_FIND_RESULT_Y2), 0, 0, 0, 0);
        return;
    }

    // MAIN
    if (g_screen == SCR_MAIN) {
        ShowWindow(g_edMainTeamName, SW_SHOW);
        ShowWindow(g_edMainTaskName, SW_SHOW);
        ShowWindow(g_edSearch, SW_SHOW);

        MoveEdit(g_edMainTeamName, SX(R_MAIN_TEAM_X1), SY(R_MAIN_TEAM_Y1),
            SX(R_MAIN_TEAM_X2), SY(R_MAIN_TEAM_Y2), 0, 0, 0, 0);

        MoveEdit(g_edMainTaskName, SX(R_MAIN_TASK_X1), SY(R_MAIN_TASK_Y1),
            SX(R_MAIN_TASK_X2), SY(R_MAIN_TASK_Y2), 0, 0, 0, 0);

        MoveEdit(g_edSearch, SX(R_TA_SEARCH_X1), SY(R_TA_SEARCH_Y1),
            SX(R_TA_SEARCH_X2), SY(R_TA_SEARCH_Y2), 0, 0, 0, 0);

        if (g_lbMyTeams && g_showMyTeams) {
            int x1 = SX(R_MYTEAM_LIST_X1);
            int y1 = SY(R_MYTEAM_LIST_Y1);
            int x2 = SX(R_MYTEAM_LIST_X2);
            int y2 = SY(R_MYTEAM_LIST_Y2);

            MoveWindow(g_lbMyTeams, x1, y1, x2 - x1, y2 - y1, TRUE);
            ShowWindow(g_lbMyTeams, SW_SHOW);
        }
        return;
    }

    // TEAM_CREATE
    if (g_screen == SCR_TEAM_CREATE) {
        ShowWindow(g_edTcTeam, SW_SHOW);
        ShowWindow(g_edTcTask, SW_SHOW);
        ShowWindow(g_edTcCode, SW_SHOW);

        MoveEdit(g_edTcTeam, SX(R_TC_TEAM_X1), SY(R_TC_TEAM_Y1),
            SX(R_TC_TEAM_X2), SY(R_TC_TEAM_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTcTask, SX(R_TC_TASK_X1), SY(R_TC_TASK_Y1),
            SX(R_TC_TASK_X2), SY(R_TC_TASK_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTcCode, SX(R_TC_CODE_X1), SY(R_TC_CODE_Y1),
            SX(R_TC_CODE_X2), SY(R_TC_CODE_Y2), 0, 0, 0, 0);
        return;
    }

    // TEAM_JOIN
    if (g_screen == SCR_TEAM_JOIN) {
        ShowWindow(g_edTjTeam, SW_SHOW);
        ShowWindow(g_edTjCode, SW_SHOW);

        MoveEdit(g_edTjTeam, SX(R_TJ_TEAM_X1), SY(R_TJ_TEAM_Y1),
            SX(R_TJ_TEAM_X2), SY(R_TJ_TEAM_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTjCode, SX(R_TJ_CODE_X1), SY(R_TJ_CODE_Y1),
            SX(R_TJ_CODE_X2), SY(R_TJ_CODE_Y2), 0, 0, 0, 0);
        return;
    }

    // TASK_ADD (✅ 오타/깨짐 수정 완료)
    if (g_screen == SCR_TASK_ADD) {
        ShowWindow(g_edTaSearch, SW_SHOW);
        ShowWindow(g_edTaTask1, SW_SHOW);
        ShowWindow(g_edTaTask2, SW_SHOW);
        ShowWindow(g_edTaTask3, SW_SHOW);
        ShowWindow(g_edTaTask4, SW_SHOW);
        ShowWindow(g_edTaTitle, SW_SHOW);
        ShowWindow(g_edTaContent, SW_SHOW);
        ShowWindow(g_edTaDetail, SW_SHOW);
        ShowWindow(g_edTaFile, SW_SHOW);

        MoveEdit(g_edTaSearch, SX(R_TA_SEARCH_X1), SY(R_TA_SEARCH_Y1),
            SX(R_TA_SEARCH_X2), SY(R_TA_SEARCH_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTask1, SX(R_TA_ITEM1_X1), SY(R_TA_ITEM1_Y1),
            SX(R_TA_ITEM1_X2), SY(R_TA_ITEM1_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTask2, SX(R_TA_ITEM2_X1), SY(R_TA_ITEM2_Y1),
            SX(R_TA_ITEM2_X2), SY(R_TA_ITEM2_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTask3, SX(R_TA_ITEM3_X1), SY(R_TA_ITEM3_Y1),
            SX(R_TA_ITEM3_X2), SY(R_TA_ITEM3_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTask4, SX(R_TA_ITEM4_X1), SY(R_TA_ITEM4_Y1),
            SX(R_TA_ITEM4_X2), SY(R_TA_ITEM4_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaTitle, SX(R_TA_TITLE_X1), SY(R_TA_TITLE_Y1),
            SX(R_TA_TITLE_X2), SY(R_TA_TITLE_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaContent, SX(R_TA_SUBDETAIL_X1), SY(R_TA_SUBDETAIL_Y1),
            SX(R_TA_SUBDETAIL_X2), SY(R_TA_SUBDETAIL_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaDetail, SX(R_TA_DETAIL_X1), SY(R_TA_DETAIL_Y1),
            SX(R_TA_DETAIL_X2), SY(R_TA_DETAIL_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaFile, SX(R_TA_FILE_X1), SY(R_TA_FILE_Y1),
            SX(R_TA_FILE_X2), SY(R_TA_FILE_Y2), 0, 0, 0, 0);
        return;
    }

    // BOARD는 board.c가 배치
    if (g_screen == SCR_BOARD) {
        Board_RelayoutControls(hWnd);
        return;
    }
}

// =========================================================
// [DO NOT TOUCH] 화면 전환
// =========================================================
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
    if (next != g_screen) NavPush(g_screen);
    SwitchScreen_NoHistory(hWnd, next);
}

static void SwitchScreen_NoHistory(HWND hWnd, Screen next)
{
    DestroyAllEdits();
    g_screen = next;

    if (next == SCR_START)            ResizeToBitmap(hWnd, g_bmpStart);
    else if (next == SCR_SIGNUP)      ResizeToBitmap(hWnd, g_bmpSignup);
    else if (next == SCR_FINDPW)      ResizeToBitmap(hWnd, g_bmpFindPw);
    else if (next == SCR_DEADLINE)    ResizeToBitmap(hWnd, g_bmpDeadline);
    else if (next == SCR_TODO)        ResizeToBitmap(hWnd, g_bmpTodo);
    else if (next == SCR_MYTEAM)      ResizeToBitmap(hWnd, g_bmpMyTeam);
    else if (next == SCR_DONE)        ResizeToBitmap(hWnd, g_bmpDone);
    else if (next == SCR_TEAM_CREATE) ResizeToBitmap(hWnd, g_bmpTeamCreate);
    else if (next == SCR_TEAM_JOIN)   ResizeToBitmap(hWnd, g_bmpTeamJoin);
    else if (next == SCR_TASK_ADD)    ResizeToBitmap(hWnd, g_bmpTaskAdd);
    else if (next == SCR_BOARD)       ResizeToBitmap(hWnd, g_bmpBoard);
    else                               ResizeToBitmap(hWnd, g_bmpMain);

    if (next != SCR_MAIN) g_showMyTeams = 0;

    CreateControlsForScreen(hWnd, next);
    RelayoutControls(hWnd);

    InvalidateRect(hWnd, NULL, FALSE);
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

    if (!g_bmpSignup || !g_bmpMain || !g_bmpFindPw ||
        !g_bmpDeadline || !g_bmpTodo || !g_bmpMyTeam || !g_bmpDone ||
        !g_bmpTeamCreate || !g_bmpTeamJoin || !g_bmpTaskAdd || !g_bmpBoard) {
        return -1;
    }

    // 내팀 리스트박스는 1번만 생성
    g_lbMyTeams = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,
        0, 0, 10, 10,
        hWnd,
        (HMENU)(INT_PTR)ID_LB_MYTEAMS,
        GetModuleHandleW(NULL),
        NULL
    );
    SendMessageW(g_lbMyTeams, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
    ShowWindow(g_lbMyTeams, SW_HIDE);

    SwitchScreen(hWnd, SCR_START);
    return 0;
}

void App_OnSize(HWND hWnd, int w, int h)
{
    g_clientW = w;
    g_clientH = h;
    RelayoutControls(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

// =========================================================
// [DO NOT TOUCH] 공통 뒤로가기
// =========================================================
static int HandleCommonBack(HWND hWnd, int x, int y)
{
    if (g_screen == SCR_START) return 0;

    if (HitScaled(R_BACK_HIT_X1, R_BACK_HIT_Y1, R_BACK_HIT_X2, R_BACK_HIT_Y2, x, y) ||
        HitScaled(R_BTN_BACK_X1, R_BTN_BACK_Y1, R_BTN_BACK_X2, R_BTN_BACK_Y2, x, y))
    {
        GoBack(hWnd);
        return 1;
    }
    return 0;
}

static void ApplyMainHeaderTextsReal(void)
{
    if (g_edMainTeamName) SetWindowTextW(g_edMainTeamName, g_mainTeamText);
    if (g_edMainTaskName) SetWindowTextW(g_edMainTaskName, g_mainTaskText);
}

// =========================================================
// 클릭 처리
// =========================================================
void App_OnLButtonDown(HWND hWnd, int x, int y)
{
    g_lastX = x;
    g_lastY = y;

    InvalidateRect(hWnd, NULL, FALSE);

    if (HandleCommonBack(hWnd, x, y)) return;

    // START
    if (g_screen == SCR_START)
    {
        if (HitScaled(R_BTN_LOGIN_X1, R_BTN_LOGIN_Y1, R_BTN_LOGIN_X2, R_BTN_LOGIN_Y2, x, y))
        {
            wchar_t id[128] = { 0 }, pw[128] = { 0 };
            GetWindowTextW(g_edStartId, id, 128);
            GetWindowTextW(g_edStartPw, pw, 128);

            if (id[0] == 0 || pw[0] == 0) {
                MessageBoxW(hWnd, L"아이디/비밀번호 입력해 주세요.", L"로그인", MB_OK | MB_ICONWARNING);
                return;
            }

            if (Auth_Login(id, pw)) SwitchScreen(hWnd, SCR_MAIN);
            else MessageBoxW(hWnd, L"로그인 실패! 아이디/비밀번호 확인해 주세요.", L"로그인", MB_OK | MB_ICONERROR);
            return;
        }

        if (HitScaled(R_BTN_TO_SIGNUP_X1, R_BTN_TO_SIGNUP_Y1, R_BTN_TO_SIGNUP_X2, R_BTN_TO_SIGNUP_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_SIGNUP); return;
        }

        if (HitScaled(R_BTN_FINDPW_X1, R_BTN_FINDPW_Y1, R_BTN_FINDPW_X2, R_BTN_FINDPW_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_FINDPW); return;
        }
        return;
    }

    // SIGNUP
    if (g_screen == SCR_SIGNUP)
    {
        if (HitScaled(R_BTN_CREATE_X1, R_BTN_CREATE_Y1, R_BTN_CREATE_X2, R_BTN_CREATE_Y2, x, y))
        {
            wchar_t name[128] = { 0 }, id[128] = { 0 }, pw[128] = { 0 };
            GetWindowTextW(g_edSignName, name, 128);
            GetWindowTextW(g_edSignId, id, 128);
            GetWindowTextW(g_edSignPw, pw, 128);

            if (name[0] == 0 || id[0] == 0 || pw[0] == 0) {
                MessageBoxW(hWnd, L"이름/아이디/비밀번호를 모두 입력해 주세요.", L"회원가입", MB_OK | MB_ICONWARNING);
                return;
            }

            if (Auth_Signup(id, pw, name)) {
                MessageBoxW(hWnd, L"회원가입 완료!", L"회원가입", MB_OK | MB_ICONINFORMATION);
                SwitchScreen(hWnd, SCR_START);
            }
            else {
                MessageBoxW(hWnd, L"회원가입 실패(이미 존재하는 아이디일 수 있음).", L"회원가입", MB_OK | MB_ICONERROR);
            }
            return;
        }
        return;
    }

    // FINDPW
    if (g_screen == SCR_FINDPW)
    {
        if (HitScaled(R_FIND_BTN_X1, R_FIND_BTN_Y1, R_FIND_BTN_X2, R_FIND_BTN_Y2, x, y))
        {
            wchar_t name[128] = { 0 }, id[128] = { 0 };
            GetWindowTextW(g_edFindName, name, 128);
            GetWindowTextW(g_edFindId, id, 128);

            if (name[0] == 0 || id[0] == 0) {
                MessageBoxW(hWnd, L"이름/아이디를 입력해 주세요.", L"비밀번호 찾기", MB_OK | MB_ICONWARNING);
                return;
            }

            wchar_t outPw[128] = { 0 };
            if (Auth_FindPassword(id, name, outPw, 128)) SetWindowTextW(g_edFindResult, outPw);
            else SetWindowTextW(g_edFindResult, L"일치하는 계정이 없습니다.");
            return;
        }
        return;
    }

    // MAIN
    if (g_screen == SCR_MAIN)
    {
        if (HitScaled(R_MAIN_BTN_DEADLINE_X1, R_MAIN_BTN_DEADLINE_Y1, R_MAIN_BTN_DEADLINE_X2, R_MAIN_BTN_DEADLINE_Y2, x, y)) { SwitchScreen(hWnd, SCR_DEADLINE); return; }
        if (HitScaled(R_MAIN_BTN_TODO_X1, R_MAIN_BTN_TODO_Y1, R_MAIN_BTN_TODO_X2, R_MAIN_BTN_TODO_Y2, x, y)) { SwitchScreen(hWnd, SCR_TODO); return; }
        if (HitScaled(R_MAIN_BTN_MYTEAM_X1, R_MAIN_BTN_MYTEAM_Y1, R_MAIN_BTN_MYTEAM_X2, R_MAIN_BTN_MYTEAM_Y2, x, y)) {
            g_showMyTeams = !g_showMyTeams;
            if (g_showMyTeams) RefreshMyTeamList(hWnd);
            RelayoutControls(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }
        if (HitScaled(R_MAIN_BTN_DONE_X1, R_MAIN_BTN_DONE_Y1, R_MAIN_BTN_DONE_X2, R_MAIN_BTN_DONE_Y2, x, y)) { SwitchScreen(hWnd, SCR_DONE); return; }

        if (HitScaled(R_MAIN_BTN_TEAM_CREATE_X1, R_MAIN_BTN_TEAM_CREATE_Y1, R_MAIN_BTN_TEAM_CREATE_X2, R_MAIN_BTN_TEAM_CREATE_Y2, x, y)) { SwitchScreen(hWnd, SCR_TEAM_CREATE); return; }
        if (HitScaled(R_MAIN_BTN_TEAM_JOIN_X1, R_MAIN_BTN_TEAM_JOIN_Y1, R_MAIN_BTN_TEAM_JOIN_X2, R_MAIN_BTN_TEAM_JOIN_Y2, x, y)) { SwitchScreen(hWnd, SCR_TEAM_JOIN); return; }
        if (HitScaled(R_MAIN_BTN_TASK_ADD_X1, R_MAIN_BTN_TASK_ADD_Y1, R_MAIN_BTN_TASK_ADD_X2, R_MAIN_BTN_TASK_ADD_Y2, x, y)) { SwitchScreen(hWnd, SCR_TASK_ADD); return; }
        if (HitScaled(R_MAIN_BTN_BOARD_X1, R_MAIN_BTN_BOARD_Y1, R_MAIN_BTN_BOARD_X2, R_MAIN_BTN_BOARD_Y2, x, y)) { SwitchScreen(hWnd, SCR_BOARD); return; }

        return;
    }

    // TEAM_CREATE
    if (g_screen == SCR_TEAM_CREATE)
    {
        if (HitScaled(R_TC_SAVE_X1, R_TC_SAVE_Y1, R_TC_SAVE_X2, R_TC_SAVE_Y2, x, y))
        {
            wchar_t team[128] = { 0 };
            wchar_t task[128] = { 0 };
            wchar_t code[128] = { 0 };

            GetWindowTextW(g_edTcTeam, team, 128);
            GetWindowTextW(g_edTcTask, task, 128);
            GetWindowTextW(g_edTcCode, code, 128);

            if (team[0] == 0 || task[0] == 0 || code[0] == 0) {
                MessageBoxW(hWnd, L"팀명/과제명/코드를 모두 입력해 주세요.", L"팀 등록", MB_OK | MB_ICONWARNING);
                return;
            }

            lstrcpynW(g_mainTeamText, team, 128);
            lstrcpynW(g_mainTaskText, task, 128);

            MessageBoxW(hWnd, L"팀 등록 완료!", L"팀 등록", MB_OK | MB_ICONINFORMATION);

            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
            return;
        }
        return;
    }

    // TEAM_JOIN
    if (g_screen == SCR_TEAM_JOIN)
    {
        if (HitScaled(R_TJ_SAVE_X1, R_TJ_SAVE_Y1, R_TJ_SAVE_X2, R_TJ_SAVE_Y2, x, y))
        {
            MessageBoxW(hWnd, L"팀 참여 완료!", L"팀 참여", MB_OK | MB_ICONINFORMATION);
            SwitchScreen(hWnd, SCR_MAIN);
            return;
        }
        return;
    }

    // ✅ BOARD: 중복 제거하고 하나로만
    if (g_screen == SCR_BOARD) {
        if (Board_OnClick(hWnd, x, y)) return;
        return;
    }
}

// =========================================================
// Paint
// =========================================================
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

    // 화면 BMP 먼저
    if (g_screen == SCR_START)       DrawBitmapFit(mem, g_bmpStart, w, h);
    else if (g_screen == SCR_SIGNUP) DrawBitmapFit(mem, g_bmpSignup, w, h);
    else if (g_screen == SCR_FINDPW) DrawBitmapFit(mem, g_bmpFindPw, w, h);
    else if (g_screen == SCR_DEADLINE)DrawBitmapFit(mem, g_bmpDeadline, w, h);
    else if (g_screen == SCR_TODO)   DrawBitmapFit(mem, g_bmpTodo, w, h);
    else if (g_screen == SCR_MYTEAM) DrawBitmapFit(mem, g_bmpMyTeam, w, h);
    else if (g_screen == SCR_DONE)   DrawBitmapFit(mem, g_bmpDone, w, h);
    else if (g_screen == SCR_TEAM_CREATE) DrawBitmapFit(mem, g_bmpTeamCreate, w, h);
    else if (g_screen == SCR_TEAM_JOIN)   DrawBitmapFit(mem, g_bmpTeamJoin, w, h);
    else if (g_screen == SCR_TASK_ADD)    DrawBitmapFit(mem, g_bmpTaskAdd, w, h);
    else if (g_screen == SCR_BOARD)       DrawBitmapFit(mem, g_bmpBoard, w, h);
    else                                  DrawBitmapFit(mem, g_bmpMain, w, h);

    // ✅ 마지막에 오버레이(라스트 클릭) 그리기
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

    if (g_bmpStart) { DeleteObject(g_bmpStart); g_bmpStart = NULL; }
    if (g_bmpSignup) { DeleteObject(g_bmpSignup); g_bmpSignup = NULL; }
    if (g_bmpMain) { DeleteObject(g_bmpMain); g_bmpMain = NULL; }
    if (g_bmpFindPw) { DeleteObject(g_bmpFindPw); g_bmpFindPw = NULL; }
    if (g_bmpDeadline) { DeleteObject(g_bmpDeadline); g_bmpDeadline = NULL; }
    if (g_bmpTodo) { DeleteObject(g_bmpTodo); g_bmpTodo = NULL; }
    if (g_bmpMyTeam) { DeleteObject(g_bmpMyTeam); g_bmpMyTeam = NULL; }
    if (g_bmpDone) { DeleteObject(g_bmpDone); g_bmpDone = NULL; }
    if (g_bmpTeamCreate) { DeleteObject(g_bmpTeamCreate); g_bmpTeamCreate = NULL; }
    if (g_bmpTeamJoin) { DeleteObject(g_bmpTeamJoin); g_bmpTeamJoin = NULL; }
    if (g_bmpTaskAdd) { DeleteObject(g_bmpTaskAdd); g_bmpTaskAdd = NULL; }
    if (g_bmpBoard) { DeleteObject(g_bmpBoard); g_bmpBoard = NULL; }

    if (g_lbMyTeams) { DestroyWindow(g_lbMyTeams); g_lbMyTeams = NULL; }
    if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = NULL; }
}

// =========================================================
// WM_COMMAND: 내팀 리스트 더블클릭 팀 전환
// =========================================================
LRESULT App_OnCommand(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;

    int id = LOWORD(wParam);
    int code = HIWORD(wParam);

    if (id == ID_LB_MYTEAMS && code == LBN_DBLCLK) {
        int sel = (int)SendMessageW(g_lbMyTeams, LB_GETCURSEL, 0, 0);
        if (sel != LB_ERR) {
            wchar_t team[64] = { 0 };
            SendMessageW(g_lbMyTeams, LB_GETTEXT, sel, (LPARAM)team);
            SwitchToTeam(hWnd, team);
        }
        return 1;
    }

    return 0;
}

// =========================================================
// 디버그 오버레이
// =========================================================
static void DrawDebugOverlay(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));  // 검정 글씨

    HFONT f = CreateFontW(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HFONT old = (HFONT)SelectObject(hdc, f);

    wchar_t buf[128];
    swprintf(buf, 128, L"Last Click: (%d, %d)  Client:(%d,%d)", g_lastX, g_lastY, g_clientW, g_clientH);
    TextOutW(hdc, 20, 20, buf, (int)wcslen(buf));

    SelectObject(hdc, old);
    DeleteObject(f);
}


// =========================================================
// 링크 에러 방지용
// =========================================================
void App_GoToStart(HWND hWnd)
{
    SwitchScreen(hWnd, SCR_START);
}

// =========================================================
// 내 팀 목록 새로고침 (현재 하드코딩)
// =========================================================
void RefreshMyTeamList(HWND hWnd)
{
    (void)hWnd;
    if (!g_lbMyTeams) return;

    SendMessageW(g_lbMyTeams, LB_RESETCONTENT, 0, 0);

    SendMessageW(g_lbMyTeams, LB_ADDSTRING, 0, (LPARAM)L"TEAM_A");
    SendMessageW(g_lbMyTeams, LB_ADDSTRING, 0, (LPARAM)L"TEAM_B");
    SendMessageW(g_lbMyTeams, LB_ADDSTRING, 0, (LPARAM)L"졸프팀");
}

// =========================================================
// 팀 전환(내팀 더블클릭)
// =========================================================
void SwitchToTeam(HWND hWnd, const wchar_t* teamId)
{
    if (!teamId || !teamId[0]) return;

    lstrcpynW(g_currentTeamId, teamId, 64);
    lstrcpynW(g_mainTeamText, teamId, 128);

    ApplyMainHeaderTextsReal();
    InvalidateRect(hWnd, NULL, TRUE);
}
