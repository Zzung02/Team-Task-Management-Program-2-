// =========================================================
// app.c  (구조 고정/정리 버전)
// - 공통 뒤로가기(투명 + UI버튼)
// - 팀등록 완료 -> 메인 상단 팀명/과제명 반영
// - 내팀 토글 + 리스트 더블클릭 팀 전환
//
// ✅ 앞으로 기능을 추가해도 "건들지 말아야 할" 핵심 구역은
//    [DO NOT TOUCH] 주석으로 표시해둠.
// =========================================================

#include "app.h"
#include "ui_coords.h"
#include "ui_layout.h"
#include "bmp_loader.h"
#include "auth.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <windowsx.h>

// =========================================================
// [DO NOT TOUCH] 좌표/히트테스트 유틸
// - 좌표 뒤집힘(X1>X2, Y1>Y2) 와 스케일링(SX/SY) 모두 안전하게 처리
// - 기능 추가해도 이건 그대로 사용하면 클릭 안정적으로 유지됨
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
// - 화면 전환 시 Edit는 Destroy되므로 "텍스트는 전역으로 유지"가 정답
// - 팀등록/팀선택 등에서 이 변수만 갱신하면 됨
// =========================================================
static wchar_t g_mainTeamText[128] = L"";
static wchar_t g_mainTaskText[128] = L"";

static void ApplyMainHeaderTexts(void)
{
    // MAIN 화면 Edit가 생성된 상태일 때만 적용됨
    extern HWND g_edMainTeamName; // 아래에서 실제 전역 static 으로 선언되어 있음(경고 억제용)
    extern HWND g_edMainTaskName;
    (void)g_edMainTeamName;
    (void)g_edMainTaskName;
    // 실제 적용은 아래의 static 핸들을 직접 사용 (컴파일러에 따라 extern 경고 방지 목적)
}

// =========================================================
// [DO NOT TOUCH] 화면 히스토리(뒤로가기)
// - 어떤 기능을 추가해도 "SwitchScreen()만 쓰면" 뒤로가기 자동 동작
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
// - Edit 위를 눌러도 App_OnLButtonDown으로 들어오게 해서
//   좌표 디버깅/클릭 일관성 유지
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

static HWND CreateEdit(HWND parent, int ctrlId, DWORD extraStyle)
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

// =========================================================
// Edit 컨트롤들(필요한 것만 유지)
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

// TEAM_JOIN (추가)
static HWND g_edTjTeam = NULL;
static HWND g_edTjCode = NULL;

// =========================================================
// 내팀 ListBox (1번만 생성)
// =========================================================
#define ID_LB_MYTEAMS  4100
static HWND g_lbMyTeams = NULL;
static int  g_showMyTeams = 0;

// ---------------------------------------------------------
// [DO NOT TOUCH] 모든 Edit 제거
// - 화면 전환 시 누수/겹침 방지
// ---------------------------------------------------------
static void DestroyAllEdits(void)
{
    if (g_edStartId) { RemovePropW(g_edStartId, PROP_OLDPROC); DestroyWindow(g_edStartId); g_edStartId = NULL; }
    if (g_edStartPw) { RemovePropW(g_edStartPw, PROP_OLDPROC); DestroyWindow(g_edStartPw); g_edStartPw = NULL; }

    if (g_edSignName) { RemovePropW(g_edSignName, PROP_OLDPROC); DestroyWindow(g_edSignName); g_edSignName = NULL; }
    if (g_edSignId) { RemovePropW(g_edSignId, PROP_OLDPROC);   DestroyWindow(g_edSignId);   g_edSignId = NULL; }
    if (g_edSignPw) { RemovePropW(g_edSignPw, PROP_OLDPROC);   DestroyWindow(g_edSignPw);   g_edSignPw = NULL; }

    if (g_edFindName) { RemovePropW(g_edFindName, PROP_OLDPROC);   DestroyWindow(g_edFindName);   g_edFindName = NULL; }
    if (g_edFindId) { RemovePropW(g_edFindId, PROP_OLDPROC);     DestroyWindow(g_edFindId);     g_edFindId = NULL; }
    if (g_edFindResult) { RemovePropW(g_edFindResult, PROP_OLDPROC); DestroyWindow(g_edFindResult); g_edFindResult = NULL; }

    if (g_edMainTeamName) { RemovePropW(g_edMainTeamName, PROP_OLDPROC); DestroyWindow(g_edMainTeamName); g_edMainTeamName = NULL; }
    if (g_edMainTaskName) { RemovePropW(g_edMainTaskName, PROP_OLDPROC); DestroyWindow(g_edMainTaskName); g_edMainTaskName = NULL; }
    if (g_edSearch) { RemovePropW(g_edSearch, PROP_OLDPROC);       DestroyWindow(g_edSearch);       g_edSearch = NULL; }

    if (g_edTcTeam) { RemovePropW(g_edTcTeam, PROP_OLDPROC); DestroyWindow(g_edTcTeam); g_edTcTeam = NULL; }
    if (g_edTcTask) { RemovePropW(g_edTcTask, PROP_OLDPROC); DestroyWindow(g_edTcTask); g_edTcTask = NULL; }
    if (g_edTcCode) { RemovePropW(g_edTcCode, PROP_OLDPROC); DestroyWindow(g_edTcCode); g_edTcCode = NULL; }

    if (g_edTjTeam) { RemovePropW(g_edTjTeam, PROP_OLDPROC); DestroyWindow(g_edTjTeam); g_edTjTeam = NULL; }
    if (g_edTjCode) { RemovePropW(g_edTjCode, PROP_OLDPROC); DestroyWindow(g_edTjCode); g_edTjCode = NULL; }
}

// =========================================================
// [DO NOT TOUCH] 화면별 컨트롤 생성
// - 여기만 건드리면 화면에 Edit 추가/삭제가 깔끔하게 관리됨
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

        // MAIN 상단 텍스트 복구(화면 전환 후에도 유지)
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

    default:
        break;
    }

    RelayoutControls(hWnd);
}

// =========================================================
// [DO NOT TOUCH] 레이아웃
// - 화면별 Edit 위치 배치
// - 기능 추가 시 "해당 화면 섹션만" 추가하면 됨
// =========================================================
static void RelayoutControls(HWND hWnd)
{
    (void)hWnd;

    // 모두 숨김
    if (g_edStartId) ShowWindow(g_edStartId, SW_HIDE);
    if (g_edStartPw) ShowWindow(g_edStartPw, SW_HIDE);

    if (g_edSignName) ShowWindow(g_edSignName, SW_HIDE);
    if (g_edSignId)   ShowWindow(g_edSignId, SW_HIDE);
    if (g_edSignPw)   ShowWindow(g_edSignPw, SW_HIDE);

    if (g_edFindName)   ShowWindow(g_edFindName, SW_HIDE);
    if (g_edFindId)     ShowWindow(g_edFindId, SW_HIDE);
    if (g_edFindResult) ShowWindow(g_edFindResult, SW_HIDE);

    if (g_edMainTeamName) ShowWindow(g_edMainTeamName, SW_HIDE);
    if (g_edMainTaskName) ShowWindow(g_edMainTaskName, SW_HIDE);
    if (g_edSearch)       ShowWindow(g_edSearch, SW_HIDE);

    if (g_edTcTeam) ShowWindow(g_edTcTeam, SW_HIDE);
    if (g_edTcTask) ShowWindow(g_edTcTask, SW_HIDE);
    if (g_edTcCode) ShowWindow(g_edTcCode, SW_HIDE);

    if (g_edTjTeam) ShowWindow(g_edTjTeam, SW_HIDE);
    if (g_edTjCode) ShowWindow(g_edTjCode, SW_HIDE);

    if (g_lbMyTeams) ShowWindow(g_lbMyTeams, SW_HIDE);

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

        // 검색창: 너가 쓰던 조회 좌표 재사용
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
}

// =========================================================
// [DO NOT TOUCH] 화면 전환
// - "SwitchScreen"만 사용하면 히스토리/레이아웃/리소스가 안정적으로 유지됨
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

    if (next == SCR_START)       ResizeToBitmap(hWnd, g_bmpStart);
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
    else                              ResizeToBitmap(hWnd, g_bmpMain);

    // MAIN 아니면 내팀 목록 자동 OFF
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
// - 어떤 기능을 추가해도 "여기만" 있으면 뒤로가기는 절대 안 깨짐
// =========================================================
static int HandleCommonBack(HWND hWnd, int x, int y)
{
    if (g_screen == SCR_START) return 0;

    // 투명 뒤로가기 영역 + UI 버튼 뒤로가기 영역(둘 다 지원)
    if (HitScaled(R_BACK_HIT_X1, R_BACK_HIT_Y1, R_BACK_HIT_X2, R_BACK_HIT_Y2, x, y) ||
        HitScaled(R_BTN_BACK_X1, R_BTN_BACK_Y1, R_BTN_BACK_X2, R_BTN_BACK_Y2, x, y))
    {
        GoBack(hWnd);
        return 1;
    }
    return 0;
}

// =========================================================
// [DO NOT TOUCH] MAIN 상단 텍스트 적용(실제 적용 함수)
// =========================================================
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

    // ✅ 공통 뒤로가기
    if (HandleCommonBack(hWnd, x, y)) return;

    // =========================================================
    // START
    // =========================================================
    if (g_screen == SCR_START)
    {
        // 로그인
        if (HitScaled(R_BTN_LOGIN_X1, R_BTN_LOGIN_Y1, R_BTN_LOGIN_X2, R_BTN_LOGIN_Y2, x, y))
        {
            wchar_t id[128] = { 0 }, pw[128] = { 0 };
            GetWindowTextW(g_edStartId, id, 128);
            GetWindowTextW(g_edStartPw, pw, 128);

            if (id[0] == 0 || pw[0] == 0) {
                MessageBoxW(hWnd, L"아이디/비밀번호 입력해 주세요.", L"로그인", MB_OK | MB_ICONWARNING);
                return;
            }

            if (Auth_Login(id, pw)) {
                SwitchScreen(hWnd, SCR_MAIN);
            }
            else {
                MessageBoxW(hWnd, L"로그인 실패! 아이디/비밀번호 확인해 주세요.", L"로그인", MB_OK | MB_ICONERROR);
            }
            return;
        }

        // 회원가입으로
        if (HitScaled(R_BTN_TO_SIGNUP_X1, R_BTN_TO_SIGNUP_Y1, R_BTN_TO_SIGNUP_X2, R_BTN_TO_SIGNUP_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_SIGNUP);
            return;
        }

        // 비번찾기로
        if (HitScaled(R_BTN_FINDPW_X1, R_BTN_FINDPW_Y1, R_BTN_FINDPW_X2, R_BTN_FINDPW_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_FINDPW);
            return;
        }

        return;
    }

    // =========================================================
    // SIGNUP
    // =========================================================
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

    // =========================================================
    // FINDPW
    // =========================================================
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
            if (Auth_FindPassword(id, name, outPw, 128)) {
                SetWindowTextW(g_edFindResult, outPw);
            }
            else {
                SetWindowTextW(g_edFindResult, L"일치하는 계정이 없습니다.");
            }
            return;
        }
        return;
    }

    // =========================================================
    // MAIN
    // =========================================================
    if (g_screen == SCR_MAIN)
    {
        if (HitScaled(R_MAIN_BTN_DEADLINE_X1, R_MAIN_BTN_DEADLINE_Y1, R_MAIN_BTN_DEADLINE_X2, R_MAIN_BTN_DEADLINE_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_DEADLINE); return;
        }
        if (HitScaled(R_MAIN_BTN_TODO_X1, R_MAIN_BTN_TODO_Y1, R_MAIN_BTN_TODO_X2, R_MAIN_BTN_TODO_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_TODO); return;
        }
        if (HitScaled(R_MAIN_BTN_MYTEAM_X1, R_MAIN_BTN_MYTEAM_Y1, R_MAIN_BTN_MYTEAM_X2, R_MAIN_BTN_MYTEAM_Y2, x, y)) {
            g_showMyTeams = !g_showMyTeams;
            if (g_showMyTeams) RefreshMyTeamList(hWnd);
            RelayoutControls(hWnd);
            InvalidateRect(hWnd, NULL, FALSE);
            return;
        }
        if (HitScaled(R_MAIN_BTN_DONE_X1, R_MAIN_BTN_DONE_Y1, R_MAIN_BTN_DONE_X2, R_MAIN_BTN_DONE_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_DONE); return;
        }

        if (HitScaled(R_MAIN_BTN_TEAM_CREATE_X1, R_MAIN_BTN_TEAM_CREATE_Y1, R_MAIN_BTN_TEAM_CREATE_X2, R_MAIN_BTN_TEAM_CREATE_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_TEAM_CREATE); return;
        }
        if (HitScaled(R_MAIN_BTN_TEAM_JOIN_X1, R_MAIN_BTN_TEAM_JOIN_Y1, R_MAIN_BTN_TEAM_JOIN_X2, R_MAIN_BTN_TEAM_JOIN_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_TEAM_JOIN); return;
        }
        if (HitScaled(R_MAIN_BTN_TASK_ADD_X1, R_MAIN_BTN_TASK_ADD_Y1, R_MAIN_BTN_TASK_ADD_X2, R_MAIN_BTN_TASK_ADD_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_TASK_ADD); return;
        }
        if (HitScaled(R_MAIN_BTN_BOARD_X1, R_MAIN_BTN_BOARD_Y1, R_MAIN_BTN_BOARD_X2, R_MAIN_BTN_BOARD_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_BOARD); return;
        }

        return;
    }

    // =========================================================
    // TEAM_CREATE (팀등록) : 저장(완료)
    // =========================================================
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

            // ✅ 팀등록 성공 시 MAIN 상단에 표시할 값 저장(핵심)
            lstrcpynW(g_mainTeamText, team, 128);
            lstrcpynW(g_mainTaskText, task, 128);

            // TODO: teams.txt 저장 로직은 여기서 추가하면 됨
            // ex) Team_Create(team, task, code);

            MessageBoxW(hWnd, L"팀 등록 완료!", L"팀 등록", MB_OK | MB_ICONINFORMATION);

            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
            return;
        }
        return;
    }

    // =========================================================
    // TEAM_JOIN (팀참여) : 저장(완료)
    // =========================================================
    if (g_screen == SCR_TEAM_JOIN)
    {
        if (HitScaled(R_TJ_SAVE_X1, R_TJ_SAVE_Y1, R_TJ_SAVE_X2, R_TJ_SAVE_Y2, x, y))
        {
            // TODO: 팀참여 로직 연결
            MessageBoxW(hWnd, L"팀 참여 완료!", L"팀 참여", MB_OK | MB_ICONINFORMATION);
            SwitchScreen(hWnd, SCR_MAIN);
            return;
        }
        return;
    }

    // =========================================================
    // (여기부터 아래는 너가 기능 추가할 때 확장하는 영역)
    // - 다른 화면(SCR_TASK_ADD, SCR_BOARD 등)의 클릭 처리 추가 가능
    // =========================================================
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

    if (g_screen == SCR_START)       DrawBitmapFit(mem, g_bmpStart, w, h);
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
    if (g_bmpDeadline) { DeleteObject(g_bmpDeadline);   g_bmpDeadline = NULL; }
    if (g_bmpTodo) { DeleteObject(g_bmpTodo);       g_bmpTodo = NULL; }
    if (g_bmpMyTeam) { DeleteObject(g_bmpMyTeam);     g_bmpMyTeam = NULL; }
    if (g_bmpDone) { DeleteObject(g_bmpDone);       g_bmpDone = NULL; }
    if (g_bmpTeamCreate) { DeleteObject(g_bmpTeamCreate); g_bmpTeamCreate = NULL; }
    if (g_bmpTeamJoin) { DeleteObject(g_bmpTeamJoin);   g_bmpTeamJoin = NULL; }
    if (g_bmpTaskAdd) { DeleteObject(g_bmpTaskAdd);    g_bmpTaskAdd = NULL; }
    if (g_bmpBoard) { DeleteObject(g_bmpBoard);      g_bmpBoard = NULL; }

    if (g_lbMyTeams) { DestroyWindow(g_lbMyTeams); g_lbMyTeams = NULL; }
    if (g_uiFont) { DeleteObject(g_uiFont);     g_uiFont = NULL; }
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

    // 팀 선택 시 상단 팀명 반영(과제명은 유지)
    lstrcpynW(g_mainTeamText, teamId, 128);

    ApplyMainHeaderTextsReal();
    InvalidateRect(hWnd, NULL, TRUE);
}
