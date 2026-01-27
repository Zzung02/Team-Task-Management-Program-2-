// app.c
#include "app.h"
#include "ui_coords.h"
#include "ui_layout.h"
#include "bmp_loader.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>

Screen g_screen = SCR_START;

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
const wchar_t* BMP_TASK_ADD = L"task_add.bmp";   // ✅ 과제등록
const wchar_t* BMP_BOARD = L"board.bmp";      // ✅ 게시판

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
HBITMAP g_bmpTaskAdd = NULL;  // ✅ 과제등록
HBITMAP g_bmpBoard = NULL;  // ✅ 게시판

// =========================================================
// [디버그/클라이언트 크기]
// =========================================================
int g_lastX = -1, g_lastY = -1;
int g_clientW = 0, g_clientH = 0;

// =========================================================
// [공통 폰트]
// =========================================================
static HFONT g_uiFont = NULL;

// =========================================================
// [텍스트 필드(Edit 컨트롤)]
//  - START / SIGNUP 화면에서만 사용
// =========================================================
static HWND g_edStartId = NULL;
static HWND g_edStartPw = NULL;

static HWND g_edSignName = NULL;
static HWND g_edSignId = NULL;
static HWND g_edSignPw = NULL;
static HWND g_edSignTeam = NULL;

// ---------------------------------------------------------
// 내부 유틸
// ---------------------------------------------------------

// ✅ 화면 전환 시 Edit가 누적 생성되는 문제를 막기 위해
//    "항상 먼저 싹 제거"하는 용도
static void DestroyAllEdits(void)
{
    if (g_edStartId) { DestroyWindow(g_edStartId);   g_edStartId = NULL; }
    if (g_edStartPw) { DestroyWindow(g_edStartPw);   g_edStartPw = NULL; }

    if (g_edSignName) { DestroyWindow(g_edSignName);  g_edSignName = NULL; }
    if (g_edSignId) { DestroyWindow(g_edSignId);    g_edSignId = NULL; }
    if (g_edSignPw) { DestroyWindow(g_edSignPw);    g_edSignPw = NULL; }
    if (g_edSignTeam) { DestroyWindow(g_edSignTeam);  g_edSignTeam = NULL; }
}

// ✅ UI 폰트 1회 생성 후 재사용
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

// ✅ Edit 생성(텍스트필드)
// - BMP에 입력 박스 테두리가 이미 있으면 WS_BORDER는 빼는 게 더 자연스러움
// - ES_AUTOHSCROLL: 길게 입력하면 옆으로 스크롤
// - ES_PASSWORD: 비밀번호 가림(PW용)
static HWND CreateEdit(HWND parent, int ctrlId, DWORD extraStyle)
{
    HWND h = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD /*| WS_BORDER*/ | ES_AUTOHSCROLL | extraStyle, // ✅ 필요하면 WS_BORDER 주석 해제
        10, 10, 100, 30,
        parent, (HMENU)(INT_PTR)ctrlId,
        GetModuleHandleW(NULL), NULL
    );

    // ✅ 폰트 적용
    HFONT f = GetUIFont();
    if (f) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);

    return h;
}

// ✅ 화면에서 Edit의 위치를 UI 좌표에 맞춰 재배치 + show/hide까지 담당
static void RelayoutControls(HWND hWnd)
{
    // ✅ [핵심] 먼저 전부 숨김 → 필요한 화면에서만 다시 Show
    if (g_edStartId)   ShowWindow(g_edStartId, SW_HIDE);
    if (g_edStartPw)   ShowWindow(g_edStartPw, SW_HIDE);
    if (g_edSignName)  ShowWindow(g_edSignName, SW_HIDE);
    if (g_edSignId)    ShowWindow(g_edSignId, SW_HIDE);
    if (g_edSignPw)    ShowWindow(g_edSignPw, SW_HIDE);
    if (g_edSignTeam)  ShowWindow(g_edSignTeam, SW_HIDE);

    // ✅ Edit 없는 화면들(BOARD 포함) → 숨긴 상태로 끝
    if (g_screen == SCR_MAIN || g_screen == SCR_FINDPW || g_screen == SCR_DEADLINE ||
        g_screen == SCR_TODO || g_screen == SCR_MYTEAM || g_screen == SCR_DONE ||
        g_screen == SCR_TEAM_CREATE || g_screen == SCR_TEAM_JOIN ||
        g_screen == SCR_TASK_ADD || g_screen == SCR_BOARD) {
        (void)hWnd;
        return;
    }

    if (g_screen == SCR_START) {
        // ✅ START에서만 표시
        if (g_edStartId) ShowWindow(g_edStartId, SW_SHOW);
        if (g_edStartPw) ShowWindow(g_edStartPw, SW_SHOW);

        MoveEdit(g_edStartId,
            SX(R_START_ID_X1), SY(R_START_ID_Y1),
            SX(R_START_ID_X2), SY(R_START_ID_Y2),
            95, 6, 12, 6);

        MoveEdit(g_edStartPw,
            SX(R_START_PW_X1), SY(R_START_PW_Y1),
            SX(R_START_PW_X2), SY(R_START_PW_Y2),
            65, 6, 12, 6);
    }
    else if (g_screen == SCR_SIGNUP) {
        // ✅ SIGNUP에서만 표시
        if (g_edSignName) ShowWindow(g_edSignName, SW_SHOW);
        if (g_edSignId)   ShowWindow(g_edSignId, SW_SHOW);
        if (g_edSignPw)   ShowWindow(g_edSignPw, SW_SHOW);
        if (g_edSignTeam) ShowWindow(g_edSignTeam, SW_SHOW);

        MoveEdit(g_edSignName,
            SX(R_SIGN_NAME_X1), SY(R_SIGN_NAME_Y1),
            SX(R_SIGN_NAME_X2), SY(R_SIGN_NAME_Y2),
            120, 6, 12, 6);

        MoveEdit(g_edSignId,
            SX(R_SIGN_ID_X1), SY(R_SIGN_ID_Y1),
            SX(R_SIGN_ID_X2), SY(R_SIGN_ID_Y2),
            120, 6, 12, 6);

        MoveEdit(g_edSignPw,
            SX(R_SIGN_PW_X1), SY(R_SIGN_PW_Y1),
            SX(R_SIGN_PW_X2), SY(R_SIGN_PW_Y2),
            120, 6, 12, 6);

        MoveEdit(g_edSignTeam,
            SX(R_SIGN_TEAM_X1), SY(R_SIGN_TEAM_Y1),
            SX(R_SIGN_TEAM_X2), SY(R_SIGN_TEAM_Y2),
            120, 6, 12, 6);
    }
}

// ✅ bmp 크기에 맞춰 창 client 영역 사이즈 조정
static void ResizeToBitmap(HWND hWnd, HBITMAP bmp)
{
    if (!bmp) return;

    BITMAP bm;
    GetObject(bmp, sizeof(bm), &bm);

    SetWindowClientSize(hWnd, bm.bmWidth, bm.bmHeight);
    g_clientW = bm.bmWidth;
    g_clientH = bm.bmHeight;
}

// ✅ 화면별로 필요한 Edit 생성
static void CreateControlsForScreen(HWND hWnd, Screen s)
{
    // ✅ [중요] SwitchScreen()에서 DestroyAllEdits()가 먼저 호출되어야 함
    //         (안 그러면 화면 전환할 때 Edit가 계속 누적 생성됨)

    // ✅ Edit 없는 화면들(BOARD 포함)
    if (s == SCR_MAIN || s == SCR_FINDPW || s == SCR_DEADLINE || s == SCR_TODO ||
        s == SCR_MYTEAM || s == SCR_DONE || s == SCR_TEAM_CREATE || s == SCR_TEAM_JOIN ||
        s == SCR_TASK_ADD || s == SCR_BOARD) {
        return;
    }

    if (s == SCR_START) {
        g_edStartId = CreateEdit(hWnd, 101, 50);
        g_edStartPw = CreateEdit(hWnd, 100, ES_PASSWORD);
    }
    else if (s == SCR_SIGNUP) {

    }
}

// ✅ 화면 전환
static void SwitchScreen(HWND hWnd, Screen next)
{
    // ✅ [핵심] 기존 Edit 제거 (전환할 때마다 누적 생성 방지)
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

    // ✅ 새 화면에 필요한 Edit 생성
    CreateControlsForScreen(hWnd, next);

    // ✅ [중요] 생성 직후 즉시 배치 + show/hide 반영
    RelayoutControls(hWnd);

    InvalidateRect(hWnd, NULL, FALSE);
}

// ✅ 디버그(라스트 클릭 표시)
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

int App_OnCreate(HWND hWnd)
{
    int w = 0, h = 0;

    g_bmpStart = LoadBmpFromExeDir(hWnd, BMP_START, &w, &h);
    if (!g_bmpStart) return -1;

    g_bmpSignup = LoadBmpFromExeDir(hWnd, BMP_SIGNUP, NULL, NULL);
    if (!g_bmpSignup) return -1;

    g_bmpMain = LoadBmpFromExeDir(hWnd, BMP_MAIN, NULL, NULL);
    if (!g_bmpMain) return -1;

    g_bmpFindPw = LoadBmpFromExeDir(hWnd, BMP_FINDPW, NULL, NULL);
    if (!g_bmpFindPw) return -1;

    g_bmpDeadline = LoadBmpFromExeDir(hWnd, BMP_DEADLINE, NULL, NULL);
    if (!g_bmpDeadline) return -1;

    g_bmpTodo = LoadBmpFromExeDir(hWnd, BMP_TODO, NULL, NULL);
    if (!g_bmpTodo) return -1;

    g_bmpMyTeam = LoadBmpFromExeDir(hWnd, BMP_MYTEAM, NULL, NULL);
    if (!g_bmpMyTeam) return -1;

    g_bmpDone = LoadBmpFromExeDir(hWnd, BMP_DONE, NULL, NULL);
    if (!g_bmpDone) return -1;

    g_bmpTeamCreate = LoadBmpFromExeDir(hWnd, BMP_TEAM_CREATE, NULL, NULL);
    if (!g_bmpTeamCreate) return -1;

    g_bmpTeamJoin = LoadBmpFromExeDir(hWnd, BMP_TEAM_JOIN, NULL, NULL);
    if (!g_bmpTeamJoin) return -1;

    g_bmpTaskAdd = LoadBmpFromExeDir(hWnd, BMP_TASK_ADD, NULL, NULL);   // ✅
    if (!g_bmpTaskAdd) return -1;

    g_bmpBoard = LoadBmpFromExeDir(hWnd, BMP_BOARD, NULL, NULL);        // ✅
    if (!g_bmpBoard) return -1;

    SwitchScreen(hWnd, SCR_START);
    return 0;
}

void App_OnSize(HWND hWnd, int w, int h)
{
    g_clientW = w;
    g_clientH = h;

    // ✅ 창 크기 바뀌면 Edit도 현재 스케일에 맞게 다시 배치
    RelayoutControls(hWnd);

    InvalidateRect(hWnd, NULL, FALSE);
    (void)hWnd;
}

void App_OnLButtonDown(HWND hWnd, int x, int y)
{
    g_lastX = x;
    g_lastY = y;

    // START
    int s_id_x1 = SX(R_START_ID_X1), s_id_y1 = SY(R_START_ID_Y1), s_id_x2 = SX(R_START_ID_X2), s_id_y2 = SY(R_START_ID_Y2);
    int s_pw_x1 = SX(R_START_PW_X1), s_pw_y1 = SY(R_START_PW_Y1), s_pw_x2 = SX(R_START_PW_X2), s_pw_y2 = SY(R_START_PW_Y2);
    int s_login_x1 = SX(R_BTN_LOGIN_X1), s_login_y1 = SY(R_BTN_LOGIN_Y1), s_login_x2 = SX(R_BTN_LOGIN_X2), s_login_y2 = SY(R_BTN_LOGIN_Y2);
    int s_to_signup_x1 = SX(R_BTN_TO_SIGNUP_X1), s_to_signup_y1 = SY(R_BTN_TO_SIGNUP_Y1),
        s_to_signup_x2 = SX(R_BTN_TO_SIGNUP_X2), s_to_signup_y2 = SY(R_BTN_TO_SIGNUP_Y2);
    int s_find_x1 = SX(R_BTN_FINDPW_X1), s_find_y1 = SY(R_BTN_FINDPW_Y1);
    int s_find_x2 = SX(R_BTN_FINDPW_X2), s_find_y2 = SY(R_BTN_FINDPW_Y2);

    // MAIN left
    int m_dead_x1 = SX(R_MAIN_BTN_DEADLINE_X1), m_dead_y1 = SY(R_MAIN_BTN_DEADLINE_Y1);
    int m_dead_x2 = SX(R_MAIN_BTN_DEADLINE_X2), m_dead_y2 = SY(R_MAIN_BTN_DEADLINE_Y2);

    int m_todo_x1 = SX(R_MAIN_BTN_TODO_X1), m_todo_y1 = SY(R_MAIN_BTN_TODO_Y1);
    int m_todo_x2 = SX(R_MAIN_BTN_TODO_X2), m_todo_y2 = SY(R_MAIN_BTN_TODO_Y2);

    int m_my_x1 = SX(R_MAIN_BTN_MYTEAM_X1), m_my_y1 = SY(R_MAIN_BTN_MYTEAM_Y1);
    int m_my_x2 = SX(R_MAIN_BTN_MYTEAM_X2), m_my_y2 = SY(R_MAIN_BTN_MYTEAM_Y2);

    int m_done_x1 = SX(R_MAIN_BTN_DONE_X1), m_done_y1 = SY(R_MAIN_BTN_DONE_Y1);
    int m_done_x2 = SX(R_MAIN_BTN_DONE_X2), m_done_y2 = SY(R_MAIN_BTN_DONE_Y2);

    // MAIN right
    int r_tc_x1 = SX(R_MAIN_BTN_TEAM_CREATE_X1), r_tc_y1 = SY(R_MAIN_BTN_TEAM_CREATE_Y1);
    int r_tc_x2 = SX(R_MAIN_BTN_TEAM_CREATE_X2), r_tc_y2 = SY(R_MAIN_BTN_TEAM_CREATE_Y2);

    int r_tj_x1 = SX(R_MAIN_BTN_TEAM_JOIN_X1), r_tj_y1 = SY(R_MAIN_BTN_TEAM_JOIN_Y1);
    int r_tj_x2 = SX(R_MAIN_BTN_TEAM_JOIN_X2), r_tj_y2 = SY(R_MAIN_BTN_TEAM_JOIN_Y2);

    int r_ta_x1 = SX(R_MAIN_BTN_TASK_ADD_X1), r_ta_y1 = SY(R_MAIN_BTN_TASK_ADD_Y1);
    int r_ta_x2 = SX(R_MAIN_BTN_TASK_ADD_X2), r_ta_y2 = SY(R_MAIN_BTN_TASK_ADD_Y2);

    // ✅ BOARD 버튼 좌표(너 ui_coords.h에 추가돼 있어야 함)
    int r_bd_x1 = SX(R_MAIN_BTN_BOARD_X1), r_bd_y1 = SY(R_MAIN_BTN_BOARD_Y1);
    int r_bd_x2 = SX(R_MAIN_BTN_BOARD_X2), r_bd_y2 = SY(R_MAIN_BTN_BOARD_Y2);

    // SIGNUP
    int b_back_x1 = SX(R_BTN_BACK_X1), b_back_y1 = SY(R_BTN_BACK_Y1), b_back_x2 = SX(R_BTN_BACK_X2), b_back_y2 = SY(R_BTN_BACK_Y2);
    int b_name_x1 = SX(R_SIGN_NAME_X1), b_name_y1 = SY(R_SIGN_NAME_Y1), b_name_x2 = SX(R_SIGN_NAME_X2), b_name_y2 = SY(R_SIGN_NAME_Y2);
    int b_id_x1 = SX(R_SIGN_ID_X1), b_id_y1 = SY(R_SIGN_ID_Y1), b_id_x2 = SX(R_SIGN_ID_X2), b_id_y2 = SY(R_SIGN_ID_Y2);
    int b_pw_x1 = SX(R_SIGN_PW_X1), b_pw_y1 = SY(R_SIGN_PW_Y1), b_pw_x2 = SX(R_SIGN_PW_X2), b_pw_y2 = SY(R_SIGN_PW_Y2);
    int b_tm_x1 = SX(R_SIGN_TEAM_X1), b_tm_y1 = SY(R_SIGN_TEAM_Y1), b_tm_x2 = SX(R_SIGN_TEAM_X2), b_tm_y2 = SY(R_SIGN_TEAM_Y2);
    int b_cr_x1 = SX(R_BTN_CREATE_X1), b_cr_y1 = SY(R_BTN_CREATE_Y1), b_cr_x2 = SX(R_BTN_CREATE_X2), b_cr_y2 = SY(R_BTN_CREATE_Y2);

    if (g_screen == SCR_START) {
        if (PtInRectXY(x, y, s_id_x1, s_id_y1, s_id_x2, s_id_y2)) { if (g_edStartId) SetFocus(g_edStartId); return; }
        if (PtInRectXY(x, y, s_pw_x1, s_pw_y1, s_pw_x2, s_pw_y2)) { if (g_edStartPw) SetFocus(g_edStartPw); return; }

        if (PtInRectXY(x, y, s_find_x1, s_find_y1, s_find_x2, s_find_y2)) { SwitchScreen(hWnd, SCR_FINDPW); return; }
        if (PtInRectXY(x, y, s_to_signup_x1, s_to_signup_y1, s_to_signup_x2, s_to_signup_y2)) { SwitchScreen(hWnd, SCR_SIGNUP); return; }
        if (PtInRectXY(x, y, s_login_x1, s_login_y1, s_login_x2, s_login_y2)) { SwitchScreen(hWnd, SCR_MAIN); return; }
        return;
    }

    if (g_screen == SCR_SIGNUP) {
        if (PtInRectXY(x, y, b_back_x1, b_back_y1, b_back_x2, b_back_y2)) { SwitchScreen(hWnd, SCR_START); return; }
        if (PtInRectXY(x, y, b_name_x1, b_name_y1, b_name_x2, b_name_y2)) { if (g_edSignName) SetFocus(g_edSignName); return; }
        if (PtInRectXY(x, y, b_id_x1, b_id_y1, b_id_x2, b_id_y2)) { if (g_edSignId) SetFocus(g_edSignId); return; }
        if (PtInRectXY(x, y, b_pw_x1, b_pw_y1, b_pw_x2, b_pw_y2)) { if (g_edSignPw) SetFocus(g_edSignPw); return; }
        if (PtInRectXY(x, y, b_tm_x1, b_tm_y1, b_tm_x2, b_tm_y2)) { if (g_edSignTeam) SetFocus(g_edSignTeam); return; }
        if (PtInRectXY(x, y, b_cr_x1, b_cr_y1, b_cr_x2, b_cr_y2)) { SwitchScreen(hWnd, SCR_MAIN); return; }
        return;
    }

    if (g_screen == SCR_MAIN) {
        // 왼쪽 필터
        if (PtInRectXY(x, y, m_dead_x1, m_dead_y1, m_dead_x2, m_dead_y2)) { SwitchScreen(hWnd, SCR_DEADLINE); return; }
        if (PtInRectXY(x, y, m_todo_x1, m_todo_y1, m_todo_x2, m_todo_y2)) { SwitchScreen(hWnd, SCR_TODO); return; }
        if (PtInRectXY(x, y, m_my_x1, m_my_y1, m_my_x2, m_my_y2)) { SwitchScreen(hWnd, SCR_MYTEAM); return; }
        if (PtInRectXY(x, y, m_done_x1, m_done_y1, m_done_x2, m_done_y2)) { SwitchScreen(hWnd, SCR_DONE); return; }

        // 오른쪽 메뉴
        if (PtInRectXY(x, y, r_tc_x1, r_tc_y1, r_tc_x2, r_tc_y2)) { SwitchScreen(hWnd, SCR_TEAM_CREATE); return; }
        if (PtInRectXY(x, y, r_tj_x1, r_tj_y1, r_tj_x2, r_tj_y2)) { SwitchScreen(hWnd, SCR_TEAM_JOIN); return; }
        if (PtInRectXY(x, y, r_ta_x1, r_ta_y1, r_ta_x2, r_ta_y2)) { SwitchScreen(hWnd, SCR_TASK_ADD); return; } // ✅
        if (PtInRectXY(x, y, r_bd_x1, r_bd_y1, r_bd_x2, r_bd_y2)) { SwitchScreen(hWnd, SCR_BOARD); return; }    // ✅ 게시판
        return;
    }

    // 임시 복귀: 다른 화면에서 아무 곳 클릭하면 메인/시작으로
    if (g_screen == SCR_DEADLINE) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_TODO) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_MYTEAM) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_DONE) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_TEAM_CREATE) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_TEAM_JOIN) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_TASK_ADD) { SwitchScreen(hWnd, SCR_MAIN);  return; }
    if (g_screen == SCR_BOARD) { SwitchScreen(hWnd, SCR_MAIN);  return; } // ✅
    if (g_screen == SCR_FINDPW) { SwitchScreen(hWnd, SCR_START); return; }
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
    else if (g_screen == SCR_BOARD)       DrawBitmapFit(mem, g_bmpBoard, w, h); // ✅
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
    // ✅ Edit 컨트롤 제거
    DestroyAllEdits();

    // ✅ BMP 해제
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
    if (g_bmpBoard) { DeleteObject(g_bmpBoard);      g_bmpBoard = NULL; } // ✅

    // ✅ 폰트 해제
    if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = NULL; }
}
