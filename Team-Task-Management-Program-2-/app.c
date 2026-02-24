

// app.c
#define _CRT_SECURE_NO_WARNINGS
#include "task.h"
#include "app.h"
#include "ui_coords.h"
#include "ui_layout.h"
#include "bmp_loader.h"
#include "auth.h"
#include "board.h"
#include "team.h"
#include "calendar.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <windowsx.h>
#include <string.h>
#include <stdlib.h>
#include <commdlg.h>   
#include <shellapi.h>  
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")
#ifndef WM_APP_CHILDCLICK
#define WM_APP_CHILDCLICK (WM_APP + 10)
#endif

static int GetMaxRightEdgeInClient(HWND hWnd, HWND* arr, int n)
{
    int maxRight = 0;
    for (int i = 0; i < n; i++) {
        if (!arr[i] || !IsWindow(arr[i])) continue;
        if (!IsWindowVisible(arr[i])) continue;

        RECT r;
        GetWindowRect(arr[i], &r);

        POINT p = { r.right, r.top };
        ScreenToClient(hWnd, &p);

        if (p.x > maxRight) maxRight = p.x;
    }
    return maxRight;
}

// ===============================
// ✅ Deadline util (YYYY/MM/DD)
// ===============================
static int ParseDateYYYYMMDD_Only(const wchar_t* s, SYSTEMTIME* out)
{
    if (!s || !s[0] || !out) return 0;

    int y = 0, m = 0, d = 0;
    if (swscanf(s, L"%d/%d/%d", &y, &m, &d) != 3) return 0;
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return 0;

    ZeroMemory(out, sizeof(*out));
    out->wYear = (WORD)y;
    out->wMonth = (WORD)m;
    out->wDay = (WORD)d;
    out->wHour = 0; out->wMinute = 0; out->wSecond = 0; out->wMilliseconds = 0;
    return 1;
}

// 오늘 기준 D-day (마감일 - 오늘). 오늘=0, 내일=1, 지났으면 음수
static int DaysUntil_YYYYMMDD(const wchar_t* deadline)
{
    SYSTEMTIME stDue, stNow;
    FILETIME ftDue, ftNow;

    if (!ParseDateYYYYMMDD_Only(deadline, &stDue)) return 999999;

    GetLocalTime(&stNow);
    stNow.wHour = 0; stNow.wMinute = 0; stNow.wSecond = 0; stNow.wMilliseconds = 0;

    if (!SystemTimeToFileTime(&stDue, &ftDue)) return 999999;
    if (!SystemTimeToFileTime(&stNow, &ftNow)) return 999999;

    ULONGLONG due = ((ULONGLONG)ftDue.dwHighDateTime << 32) | ftDue.dwLowDateTime;
    ULONGLONG now = ((ULONGLONG)ftNow.dwHighDateTime << 32) | ftNow.dwLowDateTime;

    LONGLONG diff = (LONGLONG)due - (LONGLONG)now;
    LONGLONG days = diff / (10LL * 1000 * 1000 * 60 * 60 * 24); // 100ns -> day
    return (int)days;
}

#define TASK_OWNERS_FILE L"task_owners.txt"

static HBRUSH g_brWhite = NULL;

// ---------------------------------------------------------
// Forward decl
// ---------------------------------------------------------
static void RelayoutControls(HWND hWnd);
static void ResizeToBitmap(HWND hWnd, HBITMAP bmp);
static void SwitchScreen(HWND hWnd, Screen next);
static void SwitchScreen_NoHistory(HWND hWnd, Screen next);
static void DrawDebugOverlay(HDC hdc);
static HFONT GetUIFont(void);
static void LayoutMyTeamStatics(void);
static void ApplyMyTeamTextsToUI(void);
static void HideAllControls(void);
static void ApplyCalendarClipForScreen(Screen s);

// TASK_ADD helper
static void Task_LoadToRightEdits(const TaskItem* t);
static void Task_RefreshLeftList(void);
static void Task_ClearRightEdits(void);
static int Task_FindFirstEmptyId(const wchar_t* teamId);
static void Main_RefreshBoxes(void);
static int  ParseDate_YYYYMMDD(const wchar_t* s, int* y, int* m, int* d);
static int  ParseDateToSystemTime_YYYYMMDD(const wchar_t* s, SYSTEMTIME* out);
static void Deadline_RefreshUI(void);
static void Todo_RefreshUI(void);

static int SelectFileDialog(HWND hWnd, wchar_t* outPath, int maxLen)
{
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    wchar_t buf[MAX_PATH] = L"";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return 0;

    lstrcpynW(outPath, buf, maxLen);
    return 1;
}

// ---------------------------------------------------------
// [DO NOT TOUCH] 좌표/히트테스트 유틸
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// 전역 상태
// ---------------------------------------------------------
Screen g_screen = SCR_START;

typedef enum { OVR_NONE = 0, OVR_DEADLINE = 1, OVR_UNDONE = 2 } Overlay;

static Overlay g_overlay = OVR_NONE;

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
const wchar_t* BMP_BOARD_WRITE = L"board_write.bmp";
const wchar_t* BMP_MYTEAM_DETAIL = L"myteam_detail.bmp";

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
HBITMAP g_bmpBoardWrite = NULL;
HBITMAP g_bmpMyTeamDetail = NULL;

// 디버그/클라이언트 크기
int g_lastX = -1, g_lastY = -1;
int g_clientW = 0, g_clientH = 0;

// 로그인/팀 상태
wchar_t g_currentUserId[128] = L"";
wchar_t g_currentTeamId[64] = L"";
static int g_boardEditPostId = 0;

// ---------------------------------------------------------
// MAIN 상단 텍스트 유지용
// ---------------------------------------------------------
static wchar_t g_mainTeamText[128] = L"";
static wchar_t g_mainTaskText[128] = L"";
static wchar_t g_mainCodeText[128] = L"";
static wchar_t g_selMemberUserId[128] = L"";
static HWND g_stMainCode = NULL;
static HWND g_edDoneList = NULL;
static HWND g_edOverlayList = NULL;

static HWND g_edMainTodoList = NULL;       // 미완료 과제 표시
static HWND g_edMainUrgentList = NULL;     // 마감 임박 과제 표시

static HWND g_edBwTitle = NULL;
static HWND g_edBwContent = NULL;
static HWND g_edBdSearch = NULL;   // ✅ 게시판 조회 입력칸

static int  g_taskSelectedSlot = -1;

// ---------------------------------------------------------
// 화면 히스토리(뒤로가기)  ✅ 하나만 사용
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// 폰트
// ---------------------------------------------------------
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

// ---------------------------------------------------------
// EDIT/STATIC 클릭도 Last Click 잡히게 하는 훅
// ---------------------------------------------------------
#define PROP_OLD_EDIT_PROC    L"TTM_OLD_EDIT_PROC"
#define PROP_OLD_STATIC_PROC  L"TTM_OLD_STATIC_PROC"

static LRESULT CALLBACK EditHookProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        HWND parent = GetParent(hEdit);

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(parent, &pt);

        g_lastX = pt.x;
        g_lastY = pt.y;
        InvalidateRect(parent, NULL, TRUE);
        UpdateWindow(parent);

        // ✅ 직접 호출 X -> 메시지로 우회
        PostMessageW(parent, WM_APP_CHILDCLICK, (WPARAM)pt.x, (LPARAM)pt.y);
    }

    WNDPROC oldProc = (WNDPROC)GetPropW(hEdit, PROP_OLD_EDIT_PROC);
    if (!oldProc) return DefWindowProcW(hEdit, msg, wParam, lParam);
    return CallWindowProcW(oldProc, hEdit, msg, wParam, lParam);
}

static void HookEditClick(HWND hEdit)
{
    if (!hEdit) return;

    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(
        hEdit, GWLP_WNDPROC, (LONG_PTR)EditHookProc
    );
    SetPropW(hEdit, PROP_OLD_EDIT_PROC, (HANDLE)(UINT_PTR)oldProc);
}

static LRESULT CALLBACK StaticHookProc(HWND hSt, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        HWND parent = GetParent(hSt);

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(parent, &pt);

        g_lastX = pt.x;
        g_lastY = pt.y;
        InvalidateRect(parent, NULL, TRUE);
        UpdateWindow(parent);

        // ✅ 직접 호출 X
        PostMessageW(parent, WM_APP_CHILDCLICK, (WPARAM)pt.x, (LPARAM)pt.y);

        return 0; // STATIC 클릭은 여기서 끝
    }

    WNDPROC oldProc = (WNDPROC)GetPropW(hSt, PROP_OLD_STATIC_PROC);
    if (!oldProc) return DefWindowProcW(hSt, msg, wParam, lParam);
    return CallWindowProcW(oldProc, hSt, msg, wParam, lParam);
}

static void HookStaticClick(HWND hSt)
{
    if (!hSt) return;

    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(
        hSt, GWLP_WNDPROC, (LONG_PTR)StaticHookProc
    );
    SetPropW(hSt, PROP_OLD_STATIC_PROC, (HANDLE)(UINT_PTR)oldProc);
}

// 훅 달린 윈도우 안전 파괴(원래 WndProc 복구 + Prop 제거)
static void DestroyHookedWindow(HWND* ph, const wchar_t* propName)
{
    if (!ph || !(*ph)) return;
    if (!IsWindow(*ph)) { *ph = NULL; return; }

    WNDPROC oldProc = (WNDPROC)GetPropW(*ph, propName);
    if (oldProc) {
        SetWindowLongPtrW(*ph, GWLP_WNDPROC, (LONG_PTR)oldProc);
        RemovePropW(*ph, propName);
    }
    DestroyWindow(*ph);
    *ph = NULL;
}

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

    HookEditClick(h);
    return h;
}
#define CreateEdit App_CreateEdit

// ---------------------------------------------------------
// Edit 컨트롤들
// ---------------------------------------------------------
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
static HWND g_edTcCode = NULL;

static HWND g_edTjTeam = NULL;
static HWND g_edTjCode = NULL;

// TASK_ADD
static HWND g_edTaSearch = NULL;
static HWND g_edTaTask1 = NULL;
static HWND g_edTaTask2 = NULL;
static HWND g_edTaTask3 = NULL;
static HWND g_edTaTask4 = NULL;

static HWND g_edTaTitle = NULL;
static HWND g_edTaContent = NULL;
static HWND g_edTaDetail = NULL;
static HWND g_edTaFile = NULL;
static HWND g_edTaDeadline = NULL;   // ✅ 마감일

static int  g_taskSelectedId = 0;
static int  g_taskPage = 0;

// ---------------------------------------------------------
// TASK_ADD 페이지 복원 상태
// ---------------------------------------------------------
#define TASK_PAGE_STATE_MAX 128
typedef struct {
    int used;
    int selectedSlot;
    TaskItem item;
} TaskPageState;

static TaskPageState g_taskPageState[TASK_PAGE_STATE_MAX];
static wchar_t g_taskStateTeamId[64] = L"";

static void Task_ResetPageStatesIfTeamChanged(void)
{
    if (wcscmp(g_taskStateTeamId, g_currentTeamId) != 0) {
        for (int i = 0; i < TASK_PAGE_STATE_MAX; i++) g_taskPageState[i].used = 0;
        lstrcpynW(g_taskStateTeamId, g_currentTeamId, 64);
    }
}

/* =========================
   app.c  (Part 2/4)
   ========================= */
static void Done_RefreshUI(void)
{
    if (!g_edDoneList) return;

    if (!g_currentTeamId[0]) {
        SetWindowTextW(g_edDoneList, L"팀을 먼저 선택해 주세요.");
        return;
    }

    TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
    int n = list ? Task_LoadAll(g_currentTeamId, list, 512) : 0;

    wchar_t out[8192];
    out[0] = 0;

    int k = 1;
    for (int i = 0; i < n; i++) {
        if (list[i].done == 1 && list[i].title[0] != 0) {
            wchar_t line[256];
            swprintf(line, 256, L"%d. %ls\r\n", k++, list[i].title);
            if (wcslen(out) + wcslen(line) < 8190) wcscat(out, line);
        }
    }

    if (k == 1) wcscpy(out, L"(완료된 과제가 없습니다)");
    SetWindowTextW(g_edDoneList, out);

    if (list) free(list);
}

static void Task_SaveCurrentPageState(void)
{
    if (g_taskPage < 0 || g_taskPage >= TASK_PAGE_STATE_MAX) return;

    if (g_taskSelectedId == 0 || g_taskSelectedSlot < 0 || g_taskSelectedSlot >= 4) {
        g_taskPageState[g_taskPage].used = 0;
        return;
    }

    TaskItem t = { 0 };
    t.id = g_taskSelectedId;

    if (g_edTaTitle)   GetWindowTextW(g_edTaTitle, t.title, TASK_TITLE_MAX);
    if (g_edTaContent) GetWindowTextW(g_edTaContent, t.content, TASK_TEXT_MAX);
    if (g_edTaDetail)  GetWindowTextW(g_edTaDetail, t.detail, TASK_TEXT_MAX);
    if (g_edTaDeadline) GetWindowTextW(g_edTaDeadline, t.deadline, TASK_DEADLINE_MAX);

    g_taskPageState[g_taskPage].used = 1;
    g_taskPageState[g_taskPage].selectedSlot = g_taskSelectedSlot;
    g_taskPageState[g_taskPage].item = t;
}

static void Task_RestorePageStateForPage(int page)
{
    if (page < 0 || page >= TASK_PAGE_STATE_MAX) return;

    if (!g_taskPageState[page].used) {
        g_taskSelectedSlot = -1;
        g_taskSelectedId = 0;
        return;
    }

    g_taskSelectedSlot = g_taskPageState[page].selectedSlot;
    Task_LoadToRightEdits(&g_taskPageState[page].item);
}

// ---------------------------------------------------------
// MYTEAM 슬롯
// ---------------------------------------------------------
#define MYTEAM_SLOT_MAX 8
static HWND g_stMyTeam[MYTEAM_SLOT_MAX] = { 0 };

typedef struct {
    wchar_t team[128];
    wchar_t task[128];
    wchar_t teamId[64];
    wchar_t joinCode[64];
    wchar_t role[32];          // ✅ 추가
} MyTeamInfo;

static wchar_t g_currentRole[32] = L""; // ✅ 현재 선택 팀에서 내 역할

static int IsLeaderRole(void)
{
    return (wcscmp(g_currentRole, L"LEADER") == 0 || wcscmp(g_currentRole, L"OWNER") == 0);
}

static int CanDeleteOrDone(void)
{
    return IsLeaderRole(); // 팀장만 가능
}

static MyTeamInfo g_myTeams[MYTEAM_SLOT_MAX];
static int g_myTeamSelected = -1;

static int LoadMyTeams_FromMembers(const wchar_t* userId)
{
    for (int i = 0; i < MYTEAM_SLOT_MAX; i++) {
        g_myTeams[i].team[0] = 0;
        g_myTeams[i].task[0] = 0;
        g_myTeams[i].teamId[0] = 0;
        g_myTeams[i].joinCode[0] = 0;
        g_myTeams[i].role[0] = 0;   // ✅ role도 초기화
    }

    if (!userId || !userId[0]) return 0;

    FILE* fp = NULL;

    // 1) UTF-8 시도
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");

    // 2) 실패하면 일반 r로 재시도 (ANSI/CP949/UTF-8 no BOM 대비)
    if (!fp) _wfopen_s(&fp, L"team_members.txt", L"r");

    if (!fp) return 0;

    wchar_t line[512];
    int count = 0;

    while (fgetws(line, 512, fp) && count < MYTEAM_SLOT_MAX)
    {
        wchar_t tid[64] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };

        int m = swscanf(line, L"%63[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role);
        if (m != 3) continue;
        if (wcscmp(uid, userId) != 0) continue;

        // 중복 방지
        int dup = 0;
        for (int k = 0; k < count; k++) {
            if (wcscmp(g_myTeams[k].teamId, tid) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        TeamInfo t = { 0 };
        if (Team_FindByTeamId(tid, &t)) {
            lstrcpynW(g_myTeams[count].team, t.teamName, 128);
            lstrcpynW(g_myTeams[count].teamId, t.teamId, 64);
            lstrcpynW(g_myTeams[count].joinCode, t.joinCode, 64);
            lstrcpynW(g_myTeams[count].role, role, 32);
            count++;
        }
    }

    fclose(fp);
    return count;
}

static void EnsureMyTeamStatics(HWND hWnd, HFONT font)
{
    for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
    {
        if (g_stMyTeam[i] && IsWindow(g_stMyTeam[i])) continue;
        g_stMyTeam[i] = CreateWindowExW(
            0,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY, // ✅ 여기 WS_BORDER 넣을거면 | WS_BORDER 추가
            0, 0, 10, 10,
            hWnd,
            (HMENU)(INT_PTR)(6000 + i),
            GetModuleHandleW(NULL),
            NULL
        );

        if (font) SendMessageW(g_stMyTeam[i], WM_SETFONT, (WPARAM)font, TRUE);
        HookStaticClick(g_stMyTeam[i]);
    }
}

static void ApplyMyTeamTextsToUI(void)
{
    for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
    {
        if (!g_stMyTeam[i]) continue;

        wchar_t buf[260] = { 0 };
        if (g_myTeams[i].team[0] == 0) {
            buf[0] = 0;
        }
        else {
            int isLeader =
                (wcscmp(g_myTeams[i].role, L"OWNER") == 0 ||
                    wcscmp(g_myTeams[i].role, L"LEADER") == 0);

            swprintf(buf, 260, L"%s   [코드:%s]   [%s]",
                g_myTeams[i].team,
                g_myTeams[i].joinCode,
                isLeader ? L"팀장" : L"팀원");
        }
        SetWindowTextW(g_stMyTeam[i], buf);
    }
}

static void ShowMyTeamStatics(int show)
{
    for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
    {
        if (!g_stMyTeam[i]) continue;
        ShowWindow(g_stMyTeam[i], show ? SW_SHOW : SW_HIDE);
    }
}

static void MyTeam_RefreshUI(HWND hWnd)
{
    EnsureMyTeamStatics(hWnd, GetUIFont());
    LoadMyTeams_FromMembers(g_currentUserId);
    ApplyMyTeamTextsToUI();
    LayoutMyTeamStatics();
    ShowMyTeamStatics(1);
}

// ---------------------------------------------------------
// 공통: 헤더 있는 화면?
// ---------------------------------------------------------
static int ScreenHasHeader(Screen s)
{
    switch (s) {
    case SCR_MAIN:
    case SCR_DEADLINE:
    case SCR_TODO:
    case SCR_MYTEAM:
    case SCR_MYTEAM_DETAIL:
    case SCR_DONE:
    case SCR_TEAM_CREATE:
    case SCR_TEAM_JOIN:
        return 1;

    default:
        return 0;
    }
}

static void DestroyAllEdits(void)
{
    // ✅ MAIN 박스도 같이 정리 (중요!)
    DestroyHookedWindow(&g_edMainTodoList, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edMainUrgentList, PROP_OLD_EDIT_PROC);

    // EDIT는 PROP_OLD_EDIT_PROC로 훅 정리
    DestroyHookedWindow(&g_edStartId, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edStartPw, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edSignName, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edSignId, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edSignPw, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edFindName, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edFindId, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edFindResult, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edMainTeamName, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edMainTaskName, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edSearch, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edTcTeam, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTcCode, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edTjTeam, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTjCode, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edTaSearch, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTaTitle, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTaContent, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTaDetail, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edTaFile, PROP_OLD_EDIT_PROC);

    // STATIC(왼쪽 1~4)은 PROP_OLD_STATIC_PROC로 훅 정리
    DestroyHookedWindow(&g_edTaTask1, PROP_OLD_STATIC_PROC);
    DestroyHookedWindow(&g_edTaTask2, PROP_OLD_STATIC_PROC);
    DestroyHookedWindow(&g_edTaTask3, PROP_OLD_STATIC_PROC);
    DestroyHookedWindow(&g_edTaTask4, PROP_OLD_STATIC_PROC);

    DestroyHookedWindow(&g_edTaDeadline, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edOverlayList, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edDoneList, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edBwTitle, PROP_OLD_EDIT_PROC);
    DestroyHookedWindow(&g_edBwContent, PROP_OLD_EDIT_PROC);

    DestroyHookedWindow(&g_edBdSearch, PROP_OLD_EDIT_PROC);

    Board_DestroyControls();
    ShowMyTeamStatics(0);
}

// ---------------------------------------------------------
// 헤더 텍스트 적용
// ---------------------------------------------------------
static void ApplyMainHeaderTextsReal(void)
{
    if (g_edMainTeamName) SetWindowTextW(g_edMainTeamName, g_mainTeamText);
    if (g_edMainTaskName) SetWindowTextW(g_edMainTaskName, L"");
    if (g_stMainCode) SetWindowTextW(g_stMainCode, L"");
}

static void EnsureMemberStatics(HWND hWnd, HFONT font);
static void ShowMemberStatics(int show);
static void LayoutMemberStatics(void);
static void MyTeamDetail_RefreshUI(HWND hWnd);




// ---------------------------------------------------------
// 화면별 컨트롤 생성
// ---------------------------------------------------------
static void CreateControlsForScreen(HWND hWnd, Screen s)
{

    DestroyHookedWindow(&g_edDoneList, PROP_OLD_EDIT_PROC);

    DestroyAllEdits();

    if (ScreenHasHeader(s))
    {
        g_edMainTeamName = CreateEdit(hWnd, 601, 0);
        g_edMainTaskName = CreateEdit(hWnd, 602, 0);

        if (!g_stMainCode) {
            g_stMainCode = CreateWindowExW(
                0, L"STATIC", L"",
                WS_CHILD | SS_LEFT,
                0, 0, 10, 10,
                hWnd, (HMENU)(INT_PTR)6501,
                GetModuleHandleW(NULL), NULL
            );
            SendMessageW(g_stMainCode, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
        }

        ApplyMainHeaderTextsReal();
    }

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
        g_edFindResult = CreateEdit(hWnd, 303, ES_READONLY);
        break;

    case SCR_DEADLINE:
        g_edOverlayList = CreateEdit(hWnd, 1301, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY);
        Deadline_RefreshUI();
        break;

    case SCR_TODO:
        g_edOverlayList = CreateEdit(hWnd, 1302, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY);
        Todo_RefreshUI();
        break;

    case SCR_BOARD_WRITE:
    {
        // 제목: 테두리 없이(=투명 느낌)
        g_edBwTitle = CreateEdit(hWnd, 8801, 0);
        // 내용: 멀티라인 + 스크롤
        g_edBwContent = CreateEdit(hWnd, 8802, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);

        // ✅ 제목은 테두리 제거(투명 느낌 핵심)
        LONG st = GetWindowLongW(g_edBwTitle, GWL_STYLE);
        st &= ~WS_BORDER;
        SetWindowLongW(g_edBwTitle, GWL_STYLE, st);
        // ✅ 수정 모드면 기존 글 내용 채워넣기
        if (g_boardEditPostId > 0)
        {
            wchar_t t[256] = L"", a[256] = L"", c[4096] = L"";
            if (Board_GetPostById(g_boardEditPostId, t, 256, a, 256, c, 4096)) {
                if (g_edBwTitle)   SetWindowTextW(g_edBwTitle, t);
                if (g_edBwContent) SetWindowTextW(g_edBwContent, c);
            }
        }
        else {
            // 새 글이면 비우기
            if (g_edBwTitle)   SetWindowTextW(g_edBwTitle, L"");
            if (g_edBwContent) SetWindowTextW(g_edBwContent, L"");
        }
        break;
    }


    case SCR_MAIN:
    {

        break;
    }



    case SCR_TEAM_CREATE:
        g_edTcTeam = CreateEdit(hWnd, 701, 0);
        g_edTcCode = CreateEdit(hWnd, 703, 0);
        break;

    case SCR_TEAM_JOIN:
        g_edTjTeam = CreateEdit(hWnd, 801, 0);
        g_edTjCode = CreateEdit(hWnd, 802, 0);
        break;

    case SCR_TASK_ADD:
        g_edTaTask1 = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)902, GetModuleHandleW(NULL), NULL);
        g_edTaTask2 = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)903, GetModuleHandleW(NULL), NULL);
        g_edTaTask3 = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)904, GetModuleHandleW(NULL), NULL);
        g_edTaTask4 = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFT, 0, 0, 10, 10, hWnd, (HMENU)(INT_PTR)905, GetModuleHandleW(NULL), NULL);

        SendMessageW(g_edTaTask1, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
        SendMessageW(g_edTaTask2, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
        SendMessageW(g_edTaTask3, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);
        SendMessageW(g_edTaTask4, WM_SETFONT, (WPARAM)GetUIFont(), TRUE);

        g_edTaSearch = CreateEdit(hWnd, 901, 0);
        g_edTaTitle = CreateEdit(hWnd, 906, 0);
        g_edTaContent = CreateEdit(hWnd, 907, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
        g_edTaDetail = CreateEdit(hWnd, 908, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
        g_edTaFile = CreateEdit(hWnd, 909, 0);
        g_edTaDeadline = CreateEdit(hWnd, 910, 0);
        SetWindowTextW(g_edTaDeadline, L"");   // ✅ "0000/00/00" 대신 빈칸 시작
        SendMessageW(g_edTaDeadline, EM_LIMITTEXT, 10, 0); // YYYY/MM/DD 10글자 제한(선택)


        HookStaticClick(g_edTaTask1);
        HookStaticClick(g_edTaTask2);
        HookStaticClick(g_edTaTask3);
        HookStaticClick(g_edTaTask4);

        Task_ResetPageStatesIfTeamChanged();
        g_taskPage = 0;
        g_taskSelectedSlot = -1;
        g_taskSelectedId = 0;
        Task_RefreshLeftList();
        Task_RestorePageStateForPage(g_taskPage);
        break;

    case SCR_BOARD:
    {
        // ✅ 보드 화면 들어올 때마다 팀 기준 로드
        Board_SetTeamId(g_currentTeamId);
        Board_Reload();

        // ✅ 조회 입력칸 생성
        g_edBdSearch = CreateEdit(hWnd, 8701, 0);

        break;
    }
    case SCR_MYTEAM_DETAIL:
        EnsureMemberStatics(hWnd, GetUIFont());
        MyTeamDetail_RefreshUI(hWnd);
        break;

    case SCR_MYTEAM:
        EnsureMyTeamStatics(hWnd, GetUIFont());
        MyTeam_RefreshUI(hWnd);
        break;

    case SCR_DONE:
        g_edDoneList = CreateEdit(hWnd, 1101, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY);
        Done_RefreshUI();
        break;


    default:
        break;
    }

    RelayoutControls(hWnd);
}

// ---------------------------------------------------------
// 레이아웃
// ---------------------------------------------------------
static void RelayoutControls(HWND hWnd)
{
    (void)hWnd;

    // 기본 숨김
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

    if (g_edTcTeam) ShowWindow(g_edTcTeam, SW_HIDE);
    if (g_edTcCode) ShowWindow(g_edTcCode, SW_HIDE);

    if (g_edTjTeam) ShowWindow(g_edTjTeam, SW_HIDE);
    if (g_edTjCode) ShowWindow(g_edTjCode, SW_HIDE);

    if (g_edTaSearch) ShowWindow(g_edTaSearch, SW_HIDE);
    if (g_edTaTask1) ShowWindow(g_edTaTask1, SW_HIDE);
    if (g_edTaTask2) ShowWindow(g_edTaTask2, SW_HIDE);
    if (g_edTaTask3) ShowWindow(g_edTaTask3, SW_HIDE);
    if (g_edTaTask4) ShowWindow(g_edTaTask4, SW_HIDE);
    if (g_edTaTitle) ShowWindow(g_edTaTitle, SW_HIDE);
    if (g_edTaContent) ShowWindow(g_edTaContent, SW_HIDE);
    if (g_edTaDetail) ShowWindow(g_edTaDetail, SW_HIDE);
    if (g_edTaFile) ShowWindow(g_edTaFile, SW_HIDE);
    if (g_edOverlayList) ShowWindow(g_edOverlayList, SW_HIDE);

    if (g_edBdSearch) ShowWindow(g_edBdSearch, SW_HIDE);

    ShowMemberStatics(0);
    ShowMyTeamStatics(0);

    // 헤더
    if (ScreenHasHeader(g_screen))
    {
        if (g_edMainTeamName) ShowWindow(g_edMainTeamName, SW_SHOW);
        if (g_edMainTaskName) ShowWindow(g_edMainTaskName, SW_SHOW);

        MoveEdit(g_edMainTeamName, SX(R_MAIN_TEAM_X1), SY(R_MAIN_TEAM_Y1),
            SX(R_MAIN_TEAM_X2), SY(R_MAIN_TEAM_Y2), 0, 0, 0, 0);

    }

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

    // DEADLINE
    if (g_screen == SCR_DEADLINE) {
        if (g_edOverlayList) ShowWindow(g_edOverlayList, SW_SHOW);

        MoveEdit(g_edOverlayList,
            SX(R_DEADLINE_LIST_X1), SY(R_DEADLINE_LIST_Y1),
            SX(R_DEADLINE_LIST_X2), SY(R_DEADLINE_LIST_Y2),
            0, 0, 0, 0
        );
        return;
    }

    // TODO
    if (g_screen == SCR_TODO) {
        if (g_edOverlayList) ShowWindow(g_edOverlayList, SW_SHOW);

        MoveEdit(g_edOverlayList,
            SX(R_TODO_LIST_X1), SY(R_TODO_LIST_Y1),
            SX(R_TODO_LIST_X2), SY(R_TODO_LIST_Y2),
            0, 0, 0, 0
        );
        return;
    }


    // TEAM_CREATE
    if (g_screen == SCR_TEAM_CREATE) {
        ShowWindow(g_edTcTeam, SW_SHOW);
        ShowWindow(g_edTcCode, SW_SHOW);

        MoveEdit(g_edTcTeam, SX(R_TC_TEAM_X1), SY(R_TC_TEAM_Y1),
            SX(R_TC_TEAM_X2), SY(R_TC_TEAM_Y2), 0, 0, 0, 0);

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

    // TASK_ADD
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
        ShowWindow(g_edTaDeadline, SW_SHOW);

        MoveEdit(g_edTaDeadline,
            SX(R_TA_DEADLINE_X1), SY(R_TA_DEADLINE_Y1),
            SX(R_TA_DEADLINE_X2), SY(R_TA_DEADLINE_Y2),
            0, 0, 0, 0
        );

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

        MoveEdit(g_edTaContent, SX(R_TA_CONTENT_X1), SY(R_TA_CONTENT_Y1),
            SX(R_TA_CONTENT_X2), SY(R_TA_CONTENT_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaDetail, SX(R_TA_DETAIL_X1), SY(R_TA_DETAIL_Y1),
            SX(R_TA_DETAIL_X2), SY(R_TA_DETAIL_Y2), 0, 0, 0, 0);

        MoveEdit(g_edTaFile, SX(R_TA_FILE_X1), SY(R_TA_FILE_Y1),
            SX(R_TA_FILE_X2), SY(R_TA_FILE_Y2), 0, 0, 0, 0);
        return;
    }

    if (g_screen == SCR_BOARD_WRITE)
    {
        if (g_edBwTitle)   ShowWindow(g_edBwTitle, SW_SHOW);
        if (g_edBwContent) ShowWindow(g_edBwContent, SW_SHOW);

        MoveEdit(g_edBwTitle,
            SX(R_BDW_TITLE_X1), SY(R_BDW_TITLE_Y1),
            SX(R_BDW_TITLE_X2), SY(R_BDW_TITLE_Y2),
            0, 0, 0, 0);

        MoveEdit(g_edBwContent,
            SX(R_BDW_CONTENT_X1), SY(R_BDW_CONTENT_Y1),
            SX(R_BDW_CONTENT_X2), SY(R_BDW_CONTENT_Y2),
            0, 0, 0, 0);

        return;
    }


    // BOARD
    if (g_screen == SCR_BOARD) {
        if (g_edBdSearch) ShowWindow(g_edBdSearch, SW_SHOW);

        MoveEdit(g_edBdSearch,
            SX(R_BOARD_SEARCH_X1), SY(R_BOARD_SEARCH_Y1),
            SX(R_BOARD_SEARCH_X2), SY(R_BOARD_SEARCH_Y2),
            0, 0, 0, 0);

        return;
    }

    if (g_screen == SCR_MYTEAM) {
        EnsureMyTeamStatics(hWnd, GetUIFont());  // ✅ 혹시 없으면 생성
        LayoutMyTeamStatics();                  // ✅ 먼저 위치/크기 배치
        MyTeam_RefreshUI(hWnd);                 // ✅ 텍스트 로드/적용
        ShowMyTeamStatics(1);                   // ✅ 마지막에 SHOW
        InvalidateRect(hWnd, NULL, FALSE);
        return;
    }

    if (g_screen == SCR_MYTEAM_DETAIL) {
        EnsureMemberStatics(hWnd, GetUIFont()); // (너 함수 시그니처 다르면 EnsureMemberStatics(hWnd)만)
        LayoutMemberStatics();
        MyTeamDetail_RefreshUI(hWnd);
        ShowMemberStatics(1);
        InvalidateRect(hWnd, NULL, FALSE);
        return;
    }

    // DONE
    if (g_screen == SCR_DONE) {
        if (g_edDoneList) ShowWindow(g_edDoneList, SW_SHOW);


        MoveEdit(g_edDoneList,
            SX(R_DONE_LIST_X1), SY(R_DONE_LIST_Y1),
            SX(R_DONE_LIST_X2), SY(R_DONE_LIST_Y2),
            0, 0, 0, 0
        );
        return;
    }

}

// app.c
static void HideAllControls(void)
{

    // ---------------------------
    // ✅ 아래는 app.c가 직접 만든 컨트롤들(있으면 전부 hide)
    // 너 app.c에 있는 전역 HWND 이름 그대로 맞춰서 적어줘야 함.
    // (아래는 예시니까, 네 app.c 변수명에 맞춰 바꿔야 함)
    // ---------------------------

    if (g_edStartId) ShowWindow(g_edStartId, SW_HIDE);
    if (g_edStartPw) ShowWindow(g_edStartPw, SW_HIDE);

    if (g_edSignName) ShowWindow(g_edSignName, SW_HIDE);
    if (g_edSignId)   ShowWindow(g_edSignId, SW_HIDE);
    if (g_edSignPw)   ShowWindow(g_edSignPw, SW_HIDE);

    if (g_edFindName) ShowWindow(g_edFindName, SW_HIDE);
    if (g_edFindId)   ShowWindow(g_edFindId, SW_HIDE);

    if (g_edTcTeam) ShowWindow(g_edTcTeam, SW_HIDE);
    if (g_edTcCode) ShowWindow(g_edTcCode, SW_HIDE);

    if (g_edTjTeam) ShowWindow(g_edTjTeam, SW_HIDE);
    if (g_edTjCode) ShowWindow(g_edTjCode, SW_HIDE);

    if (g_edTaSearch)   ShowWindow(g_edTaSearch, SW_HIDE);
    if (g_edTaTitle)    ShowWindow(g_edTaTitle, SW_HIDE);
    if (g_edTaContent)  ShowWindow(g_edTaContent, SW_HIDE);
    if (g_edTaDetail)   ShowWindow(g_edTaDetail, SW_HIDE);
    if (g_edTaFile)     ShowWindow(g_edTaFile, SW_HIDE);
    if (g_edTaDeadline) ShowWindow(g_edTaDeadline, SW_HIDE);

    // 내팀 STATIC 5칸 같은 거 쓰면 이것도
    // for (int i=0;i<5;i++) if (g_stMyTeam[i]) ShowWindow(g_stMyTeam[i], SW_HIDE);
}



// ---------------------------------------------------------
// 화면 전환
// ---------------------------------------------------------
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

    HideAllControls();

    if (next != g_screen) NavPush(g_screen);
    SwitchScreen_NoHistory(hWnd, next);

}

static void SwitchScreen_NoHistory(HWND hWnd, Screen next)
{
    DestroyAllEdits();
    g_screen = next;
    ApplyCalendarClipForScreen(next);
    if (next == SCR_START)            ResizeToBitmap(hWnd, g_bmpStart);
    else if (next == SCR_SIGNUP)      ResizeToBitmap(hWnd, g_bmpSignup);
    else if (next == SCR_FINDPW)      ResizeToBitmap(hWnd, g_bmpFindPw);
    else if (next == SCR_DEADLINE)    ResizeToBitmap(hWnd, g_bmpDeadline);
    else if (next == SCR_TODO)        ResizeToBitmap(hWnd, g_bmpTodo);
    else if (next == SCR_MYTEAM)      ResizeToBitmap(hWnd, g_bmpMyTeam);
    else if (next == SCR_MYTEAM_DETAIL) ResizeToBitmap(hWnd, g_bmpMyTeamDetail);
    else if (next == SCR_DONE)        ResizeToBitmap(hWnd, g_bmpDone);
    else if (next == SCR_TEAM_CREATE) ResizeToBitmap(hWnd, g_bmpTeamCreate);
    else if (next == SCR_TEAM_JOIN)   ResizeToBitmap(hWnd, g_bmpTeamJoin);
    else if (next == SCR_TASK_ADD)    ResizeToBitmap(hWnd, g_bmpTaskAdd);
    else if (next == SCR_BOARD)       ResizeToBitmap(hWnd, g_bmpBoard);

    if (next == SCR_BOARD) {
        Board_SetTeamId(g_currentTeamId);
        Board_Reload();
    }
    else if (next == SCR_BOARD_WRITE) ResizeToBitmap(hWnd, g_bmpBoardWrite);
    else                              ResizeToBitmap(hWnd, g_bmpMain);



    CreateControlsForScreen(hWnd, next);
    RelayoutControls(hWnd);
    InvalidateRect(hWnd, NULL, FALSE);
}

// ---------------------------------------------------------
// 라이프사이클
// ---------------------------------------------------------
int App_OnCreate(HWND hWnd)
{
    if (!g_brWhite) g_brWhite = CreateSolidBrush(RGB(255, 255, 255));
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
    g_bmpBoardWrite = LoadBmpFromExeDir(hWnd, BMP_BOARD_WRITE, NULL, NULL);
    g_bmpMyTeamDetail = LoadBmpFromExeDir(hWnd, BMP_MYTEAM_DETAIL, NULL, NULL);
    if (!g_bmpBoardWrite) return -1;

    if (!g_bmpSignup || !g_bmpMain || !g_bmpFindPw ||
        !g_bmpDeadline || !g_bmpTodo || !g_bmpMyTeam || !g_bmpDone ||
        !g_bmpTeamCreate || !g_bmpTeamJoin || !g_bmpTaskAdd || !g_bmpBoard) {
        return -1;
    }

    Calendar_Init();
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
// ✅ MYTEAM_DETAIL (팀원 목록)  - 단일 정의(중복 금지)
//  - 아래 블록이 app.c에 딱 1번만 존재해야 함!
// =========================================================
#define MTD_SLOT_MAX 12

typedef struct {
    wchar_t userId[128];
    wchar_t role[32]; // OWNER/LEADER/MEMBER
} MemberRow;

// ✅ 상세보기에서 볼 팀ID (내팀에서 선택한 팀)
static wchar_t g_detailTeamId[64] = L"";

static MemberRow g_mtdMembers[MTD_SLOT_MAX];
static int  g_memberCount = 0;
static int  g_memberSelected = -1;
static HWND g_stMembers[MTD_SLOT_MAX] = { 0 };

// ---------- forward decl ----------
static int  Members_Load(const wchar_t* teamId);
static int  Members_Remove(const wchar_t* teamId, const wchar_t* userId);

static void EnsureMemberStatics(HWND hWnd, HFONT font);
static void ShowMemberStatics(int show);
static void LayoutMemberStatics(void);
static void MyTeamDetail_RefreshUI(HWND hWnd);

// ---------- 구현 ----------
static void EnsureMemberStatics(HWND hWnd, HFONT font)
{
    for (int i = 0; i < MTD_SLOT_MAX; i++) {
        if (g_stMembers[i] && IsWindow(g_stMembers[i])) continue;

        g_stMembers[i] = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOTIFY,
            0, 0, 10, 10,
            hWnd, (HMENU)(INT_PTR)(7000 + i),
            GetModuleHandleW(NULL), NULL
        );

        if (font) SendMessageW(g_stMembers[i], WM_SETFONT, (WPARAM)font, TRUE);
        HookStaticClick(g_stMembers[i]);
    }
}

static void ShowMemberStatics(int show)
{
    for (int i = 0; i < MTD_SLOT_MAX; i++) {
        if (!g_stMembers[i]) continue;
        ShowWindow(g_stMembers[i], show ? SW_SHOW : SW_HIDE);
    }
}

static void LayoutMemberStatics(void)
{
    // ✅ 임시로 MYTEAM 리스트 영역 재사용 (원하면 ui_coords.h에 전용 좌표로 바꿔)
    int x1 = SX(R_MYTEAM_LIST_X1);
    int y1 = SY(R_MYTEAM_LIST_Y1);
    int x2 = SX(R_MYTEAM_LIST_X2);
    int y2 = SY(R_MYTEAM_LIST_Y2);

    int w = x2 - x1;
    int h = y2 - y1;
    if (w <= 0 || h <= 0) return;

    int slotH = h / MTD_SLOT_MAX;
    int padX = 6, padY = 6;

    for (int i = 0; i < MTD_SLOT_MAX; i++) {
        if (!g_stMembers[i]) continue;

        int left = x1 + padX;
        int top = y1 + i * slotH + padY;
        int width = w - padX * 2;
        int height = slotH - padY * 2;

        MoveWindow(g_stMembers[i], left, top, width, height, TRUE);
    }
}

static int Members_Load(const wchar_t* teamId)
{
    g_memberCount = 0;
    if (!teamId || !teamId[0]) return 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) _wfopen_s(&fp, L"team_members.txt", L"r");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp) && g_memberCount < MTD_SLOT_MAX)
    {
        wchar_t tid[64] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        if (swscanf(line, L"%63[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role) != 3) continue;
        if (wcscmp(tid, teamId) != 0) continue;

        lstrcpynW(g_mtdMembers[g_memberCount].userId, uid, 128);
        lstrcpynW(g_mtdMembers[g_memberCount].role, role, 32);
        g_memberCount++;
    }

    fclose(fp);
    return g_memberCount;
}

static void MyTeamDetail_RefreshUI(HWND hWnd)
{
    EnsureMemberStatics(hWnd, GetUIFont());

    // ✅ 로드
    Members_Load(g_detailTeamId);

    // ✅ 선택 초기화(원하면 유지해도 되는데, 일단 안전하게)
    if (g_memberSelected >= g_memberCount) g_memberSelected = -1;
    if (g_memberCount <= 0) g_memberSelected = -1;

    // ✅ 출력은 반드시 g_mtdMembers 기준!
    for (int i = 0; i < MTD_SLOT_MAX; i++) {
        if (!g_stMembers[i]) continue;

        wchar_t buf[256] = L"";
        if (i < g_memberCount) {
            swprintf(buf, 256, L"%d   [%d]",
                g_mtdMembers[i].userId,
                RoleToKorean(g_mtdMembers[i].role));
        }
        else {
            buf[0] = 0;
        }
        SetWindowTextW(g_stMembers[i], buf);
    }

    LayoutMemberStatics();
    ShowMemberStatics(1);
    InvalidateRect(hWnd, NULL, FALSE);
}





static void Member_UpdateSelectionBorder(void)
{
    for (int i = 0; i < MTD_SLOT_MAX; i++) {
        if (!g_stMembers[i]) continue;

        LONG st = GetWindowLongW(g_stMembers[i], GWL_STYLE);

        if (i == g_memberSelected) st |= WS_BORDER;   // ✅ 선택된 줄만 테두리 ON
        else                       st &= ~WS_BORDER;  // ✅ 나머지는 OFF

        SetWindowLongW(g_stMembers[i], GWL_STYLE, st);

        // 스타일 변경 후 즉시 반영
        SetWindowPos(g_stMembers[i], NULL, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        InvalidateRect(g_stMembers[i], NULL, TRUE);
    }
}


static int Members_UpdateRole_Legacy(const wchar_t* teamId, const wchar_t* userId, const wchar_t* newRole)
{
    if (!teamId || !teamId[0] || !userId || !userId[0] || !newRole || !newRole[0]) return 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) _wfopen_s(&fp, L"team_members.txt", L"r");
    if (!fp) return 0;

    // 라인 전체를 저장(최대 1024줄 정도로 제한)
    wchar_t lines[2048][512];
    int count = 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp) && count < 2048) {
        lstrcpynW(lines[count], line, 512);
        count++;
    }
    fclose(fp);

    int changed = 0;

    for (int i = 0; i < count; i++) {
        wchar_t tid[64] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        if (swscanf(lines[i], L"%63[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role) != 3) continue;

        if (wcscmp(tid, teamId) == 0 && wcscmp(uid, userId) == 0) {
            // 기존 줄을 역할 변경해서 덮어쓰기
            swprintf(lines[i], 512, L"%s|%s|%s\n", teamId, userId, newRole);
            changed = 1;
            break;
        }
    }

    if (!changed) return 0; // 대상 멤버를 못 찾음

    _wfopen_s(&fp, L"team_members.txt", L"w, ccs=UTF-8");
    if (!fp) return 0;

    for (int i = 0; i < count; i++) {
        fputws(lines[i], fp);
    }
    fclose(fp);

    // UI 반영용: 메모리에도 업데이트
    for (int i = 0; i < g_memberCount; i++) {
        if (wcscmp(g_mtdMembers[i].userId, userId) == 0) {
            lstrcpynW(g_mtdMembers[i].role, newRole, 32);
            break;
        }
    }

    return 1;
}


// team_members.txt 전체를 읽어서 수정 후 재저장하는 헬퍼
typedef struct {
    wchar_t tid[64];
    wchar_t uid[128];
    wchar_t role[32];
} TMRow;

static int ReadAllTeamMembers(TMRow** outRows, int* outCount)
{
    if (!outRows || !outCount) return 0;
    *outRows = NULL; *outCount = 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) _wfopen_s(&fp, L"team_members.txt", L"r");
    if (!fp) return 1; // 파일 없으면 비어있다고 보고 성공

    int cap = 128, n = 0;
    TMRow* rows = (TMRow*)calloc(cap, sizeof(TMRow));
    if (!rows) { fclose(fp); return 0; }

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[64] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        if (swscanf(line, L"%63[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role) != 3) continue;

        if (n >= cap) {
            cap *= 2;
            TMRow* nr = (TMRow*)realloc(rows, sizeof(TMRow) * cap);
            if (!nr) { free(rows); fclose(fp); return 0; }
            rows = nr;
            ZeroMemory(&rows[n], sizeof(TMRow) * (cap - n));
        }

        lstrcpynW(rows[n].tid, tid, 64);
        lstrcpynW(rows[n].uid, uid, 128);
        lstrcpynW(rows[n].role, role, 32);
        n++;
    }

    fclose(fp);
    *outRows = rows;
    *outCount = n;
    return 1;
}

static int WriteAllTeamMembers(const TMRow* rows, int count)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"w, ccs=UTF-8");
    if (!fp) return 0;

    for (int i = 0; i < count; i++) {
        fwprintf(fp, L"%s|%s|%s\n", rows[i].tid, rows[i].uid, rows[i].role);
    }
    fclose(fp);
    return 1;
}

// ✅ 역할 변경 (OWNER는 변경 금지)
static int Members_UpdateRole(const wchar_t* teamId, const wchar_t* userId, const wchar_t* newRole)
{
    if (!teamId || !teamId[0] || !userId || !userId[0] || !newRole || !newRole[0]) return 0;

    TMRow* rows = NULL; int n = 0;
    if (!ReadAllTeamMembers(&rows, &n)) return 0;

    int changed = 0;
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;
        if (wcscmp(rows[i].uid, userId) != 0) continue;

        if (wcscmp(rows[i].role, L"OWNER") == 0) { // OWNER 보호
            if (rows) free(rows);
            return 0;
        }
        lstrcpynW(rows[i].role, newRole, 32);
        changed = 1;
        break;
    }

    int ok = 1;
    if (changed) ok = WriteAllTeamMembers(rows, n);

    if (rows) free(rows);
    return ok && changed;
}

// ✅ 팀장 위임: newLeader를 LEADER로, 기존 LEADER(OWNER 제외)는 MEMBER로 내림
static int Members_DelegateLeader(const wchar_t* teamId, const wchar_t* newLeaderUserId)
{
    if (!teamId || !teamId[0] || !newLeaderUserId || !newLeaderUserId[0]) return 0;

    TMRow* rows = NULL; int n = 0;
    if (!ReadAllTeamMembers(&rows, &n)) return 0;

    // newLeader가 이 팀 멤버인지 확인 + OWNER면 불가(OWNER는 바꾸지 않는다는 정책)
    int foundNew = 0;
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;
        if (wcscmp(rows[i].uid, newLeaderUserId) != 0) continue;

        if (wcscmp(rows[i].role, L"OWNER") == 0) { // OWNER에게 위임은 의미 없음(이미 최고)
            if (rows) free(rows);
            return 0;
        }
        foundNew = 1;
        break;
    }
    if (!foundNew) { if (rows) free(rows); return 0; }

    // 1) 기존 LEADER들(OWNER 제외) -> MEMBER
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;
        if (wcscmp(rows[i].role, L"LEADER") == 0) {
            lstrcpynW(rows[i].role, L"MEMBER", 32);
        }
    }

    // 2) newLeader -> LEADER
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;
        if (wcscmp(rows[i].uid, newLeaderUserId) != 0) continue;
        lstrcpynW(rows[i].role, L"LEADER", 32);
        break;
    }

    int ok = WriteAllTeamMembers(rows, n);
    if (rows) free(rows);
    return ok;
}

// ✅ 멤버 삭제: teamId|userId 라인 제거 (OWNER는 삭제 금지)
static int Members_Remove(const wchar_t* teamId, const wchar_t* userId)
{
    if (!teamId || !teamId[0] || !userId || !userId[0]) return 0;

    TMRow* rows = NULL; int n = 0;
    if (!ReadAllTeamMembers(&rows, &n)) return 0;

    // 새 배열로 필터링
    TMRow* out = (TMRow*)calloc(n, sizeof(TMRow));
    if (!out) { if (rows) free(rows); return 0; }

    int outN = 0;
    int removed = 0;

    for (int i = 0; i < n; i++) {
        int isTarget = (wcscmp(rows[i].tid, teamId) == 0 && wcscmp(rows[i].uid, userId) == 0);
        if (isTarget) {
            if (wcscmp(rows[i].role, L"OWNER") == 0) {
                // OWNER는 삭제 불가 -> 그대로 유지
                out[outN++] = rows[i];
            }
            else {
                removed = 1; // 제거
            }
        }
        else {
            out[outN++] = rows[i];
        }
    }

    int ok = 1;
    if (removed) ok = WriteAllTeamMembers(out, outN);

    free(out);
    if (rows) free(rows);
    return ok && removed;
}



// ---------------------------------------------------------
// 클릭 처리
// ---------------------------------------------------------
void App_OnLButtonDown(HWND hWnd, int x, int y)
{
    static int s_in = 0;
    if (s_in) return;
    s_in = 1;

#define SAFE_LEAVE() do { s_in = 0; return; } while (0)

    g_lastX = x;
    g_lastY = y;
    InvalidateRect(hWnd, NULL, FALSE);

    // ✅ 공통 뒤로가기 버튼 (START는 제외)
    if (g_screen != SCR_START &&
        HitScaled(R_BTN_BACK_GLOBAL_X1, R_BTN_BACK_GLOBAL_Y1,
            R_BTN_BACK_GLOBAL_X2, R_BTN_BACK_GLOBAL_Y2, x, y))
    {
        GoBack(hWnd);
        SAFE_LEAVE();
    }

    // -----------------------------------------------------
    // START
    // -----------------------------------------------------
    if (g_screen == SCR_START)
    {
        if (HitScaled(R_BTN_LOGIN_X1, R_BTN_LOGIN_Y1, R_BTN_LOGIN_X2, R_BTN_LOGIN_Y2, x, y))
        {
            wchar_t id[128] = { 0 }, pw[128] = { 0 };
            GetWindowTextW(g_edStartId, id, 128);
            GetWindowTextW(g_edStartPw, pw, 128);

            if (id[0] == 0 || pw[0] == 0) {
                MessageBoxW(hWnd, L"아이디/비밀번호 입력해 주세요.", L"로그인", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (Auth_Login(id, pw)) {

                // ✅ 로그인 아이디 저장 (이거 없어서 내팀 목록이 비어있던거!)
                lstrcpynW(g_currentUserId, id, 128);

                // (선택) 로그인 시 팀/역할 초기화
                // g_currentTeamId[0] = 0;
                // g_currentRole[0] = 0;

                Calendar_Init();
                Calendar_SetTeamId(g_currentTeamId);
                Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

                SwitchScreen(hWnd, SCR_MAIN);
                SAFE_LEAVE();
            }

            else {
                MessageBoxW(hWnd, L"로그인 실패! 아이디/비밀번호 확인해 주세요.", L"로그인", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }
        }

        if (HitScaled(R_BTN_TO_SIGNUP_X1, R_BTN_TO_SIGNUP_Y1, R_BTN_TO_SIGNUP_X2, R_BTN_TO_SIGNUP_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_SIGNUP);
            SAFE_LEAVE();
        }

        if (HitScaled(R_BTN_FINDPW_X1, R_BTN_FINDPW_Y1, R_BTN_FINDPW_X2, R_BTN_FINDPW_Y2, x, y)) {
            SwitchScreen(hWnd, SCR_FINDPW);
            SAFE_LEAVE();
        }

        SAFE_LEAVE();
    }

    // -----------------------------------------------------
    // SIGNUP
    // -----------------------------------------------------
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
                SAFE_LEAVE();
            }

            if (Auth_Signup(id, pw, name)) {
                MessageBoxW(hWnd, L"회원가입 완료!", L"회원가입", MB_OK | MB_ICONINFORMATION);
                SwitchScreen(hWnd, SCR_START);
                SAFE_LEAVE();
            }
            else {
                MessageBoxW(hWnd, L"회원가입 실패(이미 존재하는 아이디일 수 있음).", L"회원가입", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }
        }

        SAFE_LEAVE();
    }

    // -----------------------------------------------------
    // FINDPW
    // -----------------------------------------------------
    if (g_screen == SCR_FINDPW)
    {
        if (HitScaled(R_FIND_BTN_X1, R_FIND_BTN_Y1, R_FIND_BTN_X2, R_FIND_BTN_Y2, x, y))
        {
            wchar_t name[128] = { 0 }, id[128] = { 0 };
            GetWindowTextW(g_edFindName, name, 128);
            GetWindowTextW(g_edFindId, id, 128);

            if (name[0] == 0 || id[0] == 0) {
                MessageBoxW(hWnd, L"이름/아이디를 입력해 주세요.", L"비밀번호 찾기", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            wchar_t outPw[128] = { 0 };
            if (Auth_FindPassword(id, name, outPw, 128)) SetWindowTextW(g_edFindResult, outPw);
            else SetWindowTextW(g_edFindResult, L"일치하는 계정이 없습니다.");

            SAFE_LEAVE();
        }
        SAFE_LEAVE();
    }

    // -----------------------------------------------------
// MAIN
// -----------------------------------------------------
    if (g_screen == SCR_MAIN)
    {
        // 1) 먼저 버튼/메뉴 클릭 처리
        if (HitScaled(R_MAIN_BTN_DEADLINE_X1, R_MAIN_BTN_DEADLINE_Y1,
            R_MAIN_BTN_DEADLINE_X2, R_MAIN_BTN_DEADLINE_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_DEADLINE);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_TODO_X1, R_MAIN_BTN_TODO_Y1,
            R_MAIN_BTN_TODO_X2, R_MAIN_BTN_TODO_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_TODO);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_MYTEAM_X1, R_MAIN_BTN_MYTEAM_Y1,
            R_MAIN_BTN_MYTEAM_X2, R_MAIN_BTN_MYTEAM_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_MYTEAM);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_DONE_X1, R_MAIN_BTN_DONE_Y1,
            R_MAIN_BTN_DONE_X2, R_MAIN_BTN_DONE_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_DONE);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_TEAM_CREATE_X1, R_MAIN_BTN_TEAM_CREATE_Y1,
            R_MAIN_BTN_TEAM_CREATE_X2, R_MAIN_BTN_TEAM_CREATE_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_TEAM_CREATE);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_TEAM_JOIN_X1, R_MAIN_BTN_TEAM_JOIN_Y1,
            R_MAIN_BTN_TEAM_JOIN_X2, R_MAIN_BTN_TEAM_JOIN_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_TEAM_JOIN);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_TASK_ADD_X1, R_MAIN_BTN_TASK_ADD_Y1,
            R_MAIN_BTN_TASK_ADD_X2, R_MAIN_BTN_TASK_ADD_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_TASK_ADD);
            SAFE_LEAVE();
        }

        if (HitScaled(R_MAIN_BTN_BOARD_X1, R_MAIN_BTN_BOARD_Y1,
            R_MAIN_BTN_BOARD_X2, R_MAIN_BTN_BOARD_Y2, x, y))
        {
            SwitchScreen(hWnd, SCR_BOARD);
            SAFE_LEAVE();
        }

        // 2) 버튼에 안 걸렸을 때만 캘린더 클릭 처리
        if (Calendar_OnClick(hWnd, x, y)) SAFE_LEAVE();
        // ✅ clipX도 반영 (왼쪽 UI 겹치는 구간 클릭 금지)

        SAFE_LEAVE();
    }


    // -----------------------------------------------------
    // TEAM_CREATE
    // -----------------------------------------------------
    if (g_screen == SCR_TEAM_CREATE)
    {
        if (HitScaled(R_TC_SAVE_X1, R_TC_SAVE_Y1, R_TC_SAVE_X2, R_TC_SAVE_Y2, x, y))
        {
            wchar_t team[128] = { 0 };
            wchar_t code[128] = { 0 };

            GetWindowTextW(g_edTcTeam, team, 128);
            GetWindowTextW(g_edTcCode, code, 128);

            if (team[0] == 0 || code[0] == 0) {
                MessageBoxW(hWnd, L"팀명/코드를 모두 입력해 주세요.", L"팀 등록", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TeamInfo out = { 0 };
            if (!Team_Create(team, code, g_currentUserId, &out)) {
                MessageBoxW(hWnd, L"팀 등록 실패!\n(코드 중복이거나 파일 저장 오류일 수 있음)", L"팀 등록", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            lstrcpynW(g_currentTeamId, out.teamId, 64);
            lstrcpynW(g_mainTeamText, out.teamName, 128);
            lstrcpynW(g_currentRole, L"LEADER", 32);

            // ✅ 팀 바뀌었으니 보드/캘린더도 팀 세팅
            Board_SetTeamId(g_currentTeamId);
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

            LoadMyTeams_FromMembers(g_currentUserId);
            ApplyMyTeamTextsToUI();

            MessageBoxW(hWnd, L"팀 등록 완료!", L"팀 등록", MB_OK | MB_ICONINFORMATION);
            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
            SAFE_LEAVE();
        }
        SAFE_LEAVE();
    }

    // -----------------------------------------------------
    // TEAM_JOIN
    // -----------------------------------------------------
    if (g_screen == SCR_TEAM_JOIN)
    {
        if (HitScaled(R_TJ_SAVE_X1, R_TJ_SAVE_Y1, R_TJ_SAVE_X2, R_TJ_SAVE_Y2, x, y))
        {
            wchar_t joinCode[128] = { 0 };
            GetWindowTextW(g_edTjCode, joinCode, 128);

            if (joinCode[0] == 0) {
                MessageBoxW(hWnd, L"참여 코드를 입력해 주세요.", L"팀 참여", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }
            if (g_currentUserId[0] == 0) {
                MessageBoxW(hWnd, L"로그인 정보가 없습니다. 다시 로그인해 주세요.", L"팀 참여", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            TeamInfo out = { 0 };
            if (!Team_JoinByCode(joinCode, g_currentUserId, &out)) {
                MessageBoxW(hWnd, L"팀 참여 실패!\n(코드가 없거나 이미 가입했을 수 있음)", L"팀 참여", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }
            lstrcpynW(g_currentTeamId, out.teamId, 64);
            lstrcpynW(g_mainTeamText, out.teamName, 128);
            lstrcpynW(g_currentRole, L"MEMBER", 32);

            // ✅ 팀 바뀌었으니 보드/캘린더도 팀 세팅
            Board_SetTeamId(g_currentTeamId);
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);
            LoadMyTeams_FromMembers(g_currentUserId);
            ApplyMyTeamTextsToUI();

            MessageBoxW(hWnd, L"팀 참여 완료!", L"팀 참여", MB_OK | MB_ICONINFORMATION);
            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
            SAFE_LEAVE();
        }
        SAFE_LEAVE();
    }

    // -----------------------------------------------------
    // MYTEAM
    // -----------------------------------------------------
    if (g_screen == SCR_MYTEAM)
    {
        if (HitScaled(R_MYTEAM_SAVE_X1, R_MYTEAM_SAVE_Y1, R_MYTEAM_SAVE_X2, R_MYTEAM_SAVE_Y2, x, y))
        {
            if (g_myTeamSelected < 0 || g_myTeamSelected >= MYTEAM_SLOT_MAX ||
                g_myTeams[g_myTeamSelected].team[0] == 0)
            {
                MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요.", L"내 팀", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }
            lstrcpynW(g_mainTeamText, g_myTeams[g_myTeamSelected].team, 128);
            lstrcpynW(g_currentTeamId, g_myTeams[g_myTeamSelected].teamId, 64);
            lstrcpynW(g_currentRole, g_myTeams[g_myTeamSelected].role, 32);

            // ✅ 팀 바뀌었으니 보드/캘린더도 팀 세팅
            Board_SetTeamId(g_currentTeamId);
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
            SAFE_LEAVE();
        }

        // ✅ 상세보기 버튼 (좌표는 ui_coords.h에 만들어야 함)
        if (HitScaled(R_MYTEAM_DETAIL_X1, R_MYTEAM_DETAIL_Y1,
            R_MYTEAM_DETAIL_X2, R_MYTEAM_DETAIL_Y2, x, y))
        {
            // 팀 선택 안 했으면 막기
            if (g_myTeamSelected < 0 || g_myTeamSelected >= MYTEAM_SLOT_MAX ||
                g_myTeams[g_myTeamSelected].teamId[0] == 0)
            {
                MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요.", L"상세보기", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }
            // ✅ 상세 화면이 볼 팀 id 저장 (가장 중요!)
            lstrcpynW(g_detailTeamId, g_myTeams[g_myTeamSelected].teamId, 64);

            // ✅ 상세보기 화면에서도 "내 역할"이 정확해야 권한 버튼이 동작함
            lstrcpynW(g_currentTeamId, g_detailTeamId, 64);
            lstrcpynW(g_currentRole, g_myTeams[g_myTeamSelected].role, 32);

            SwitchScreen(hWnd, SCR_MYTEAM_DETAIL);
            SAFE_LEAVE();
        }

        for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
        {
            if (!g_stMyTeam[i]) continue;
            if (g_myTeams[i].team[0] == 0) continue;

            RECT r;
            GetWindowRect(g_stMyTeam[i], &r);

            POINT p1 = { r.left, r.top };
            POINT p2 = { r.right, r.bottom };
            ScreenToClient(hWnd, &p1);
            ScreenToClient(hWnd, &p2);

            RECT rcSlot = { p1.x, p1.y, p2.x, p2.y };
            POINT pt = { x, y };

            if (PtInRect(&rcSlot, pt))
            {
                g_myTeamSelected = i;
                InvalidateRect(hWnd, NULL, FALSE);
                SAFE_LEAVE();
            }
        }

        SAFE_LEAVE();
    }




    // -----------------------------------------------------
 // MYTEAM_DETAIL
 // -----------------------------------------------------
    if (g_screen == SCR_MYTEAM_DETAIL)
    {
        POINT pt = { x, y };

        // ------------------------------------
        // 1) 멤버 클릭 → 선택 테두리
        // ------------------------------------
        for (int i = 0; i < g_memberCount; i++)
        {
            if (!g_stMembers[i]) continue;

            RECT r;
            GetWindowRect(g_stMembers[i], &r);

            POINT p1 = { r.left, r.top };
            POINT p2 = { r.right, r.bottom };
            ScreenToClient(hWnd, &p1);
            ScreenToClient(hWnd, &p2);

            RECT rcSlot = { p1.x, p1.y, p2.x, p2.y };

            if (PtInRect(&rcSlot, pt))
            {
                g_memberSelected = i;
                Member_UpdateSelectionBorder();
                InvalidateRect(hWnd, NULL, FALSE);
                SAFE_LEAVE();
            }
        }

        // ------------------------------------
        // 2) 멤버 삭제 (팀장만 가능)
        // ------------------------------------
        if (HitScaled(R_MTD_DEL_X1, R_MTD_DEL_Y1,
            R_MTD_DEL_X2, R_MTD_DEL_Y2, x, y))
        {
            if (!IsLeaderRole()) {
                MessageBoxW(hWnd, L"팀장만 삭제할 수 있습니다.",
                    L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (g_memberSelected < 0 || g_memberSelected >= g_memberCount) {
                MessageBoxW(hWnd, L"삭제할 팀원을 선택해 주세요.",
                    L"알림", MB_OK | MB_ICONINFORMATION);
                SAFE_LEAVE();
            }

            const wchar_t* targetId = g_mtdMembers[g_memberSelected].userId;
            const wchar_t* role = g_mtdMembers[g_memberSelected].role;

            // OWNER 삭제 금지
            if (wcscmp(role, L"OWNER") == 0) {
                MessageBoxW(hWnd, L"OWNER는 삭제할 수 없습니다.",
                    L"알림", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (MessageBoxW(hWnd, L"정말 삭제할까요?",
                L"삭제", MB_YESNO | MB_ICONQUESTION) != IDYES)
                SAFE_LEAVE();

            if (!Members_Remove(g_detailTeamId, targetId)) {
                MessageBoxW(hWnd, L"삭제 실패",
                    L"오류", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            MessageBoxW(hWnd, L"삭제 완료!",
                L"내 팀", MB_OK | MB_ICONINFORMATION);

            MyTeamDetail_RefreshUI(hWnd);
            SAFE_LEAVE();
        }

        // ------------------------------------
        // 3) 팀장 자동 위임 (참여 순)
        // ------------------------------------
        if (HitScaled(R_MTD_SWAP_X1, R_MTD_SWAP_Y1,
            R_MTD_SWAP_X2, R_MTD_SWAP_Y2, x, y))
        {
            if (!IsLeaderRole()) {
                MessageBoxW(hWnd, L"팀장만 위임 가능합니다.",
                    L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (!Members_TransferOwnerAuto(g_detailTeamId, g_currentUserId)) {
                MessageBoxW(hWnd,
                    L"위임 실패 (위임할 팀원이 없거나 오류)",
                    L"오류", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            MessageBoxW(hWnd, L"팀장 위임 완료!",
                L"내 팀", MB_OK | MB_ICONINFORMATION);

            SwitchScreen(hWnd, SCR_MYTEAM);
            SAFE_LEAVE();
        }

        SAFE_LEAVE();
    }

    if (g_screen == SCR_TASK_ADD)
    {
        // ✅ 왼쪽 과제목록(1~4) 클릭 -> 선택 처리 (여기 안으로 이동!)
        if (HitScaled(R_TA_ITEM1_X1, R_TA_ITEM1_Y1, R_TA_ITEM1_X2, R_TA_ITEM1_Y2, x, y) ||
            HitScaled(R_TA_ITEM2_X1, R_TA_ITEM2_Y1, R_TA_ITEM2_X2, R_TA_ITEM2_Y2, x, y) ||
            HitScaled(R_TA_ITEM3_X1, R_TA_ITEM3_Y1, R_TA_ITEM3_X2, R_TA_ITEM3_Y2, x, y) ||
            HitScaled(R_TA_ITEM4_X1, R_TA_ITEM4_Y1, R_TA_ITEM4_X2, R_TA_ITEM4_Y2, x, y))
        {
            int slot = -1;
            if (HitScaled(R_TA_ITEM1_X1, R_TA_ITEM1_Y1, R_TA_ITEM1_X2, R_TA_ITEM1_Y2, x, y)) slot = 0;
            else if (HitScaled(R_TA_ITEM2_X1, R_TA_ITEM2_Y1, R_TA_ITEM2_X2, R_TA_ITEM2_Y2, x, y)) slot = 1;
            else if (HitScaled(R_TA_ITEM3_X1, R_TA_ITEM3_Y1, R_TA_ITEM3_X2, R_TA_ITEM3_Y2, x, y)) slot = 2;
            else if (HitScaled(R_TA_ITEM4_X1, R_TA_ITEM4_Y1, R_TA_ITEM4_X2, R_TA_ITEM4_Y2, x, y)) slot = 3;

            if (slot >= 0 && g_currentTeamId[0])
            {
                TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
                int n = list ? Task_LoadActiveOnly(g_currentTeamId, list, 512) : 0;

                int idx = g_taskPage * 4 + slot;
                if (idx >= 0 && idx < n)
                {
                    g_taskSelectedSlot = slot;
                    g_taskSelectedId = list[idx].id;
                    Task_LoadToRightEdits(&list[idx]);
                    Task_SaveCurrentPageState();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                else
                {
                    g_taskSelectedSlot = -1;
                    g_taskSelectedId = 0;
                    Task_ClearRightEdits();
                    InvalidateRect(hWnd, NULL, FALSE);

                }

                if (list) free(list);
            }
            SAFE_LEAVE();
        }

        // (그 다음에 삭제/조회/등록/수정/완료/페이지 이동/파일선택/다운로드 처리 계속...)
    }



    // -----------------------------------------------------
    // TASK_ADD ✅ 화면 체크로 전부 묶음
    // -----------------------------------------------------
  // 삭제
    if (g_screen == SCR_TASK_ADD)
    {
        if (HitScaled(R_TA_BTN_DEL_X1, R_TA_BTN_DEL_Y1,
            R_TA_BTN_DEL_X2, R_TA_BTN_DEL_Y2, x, y))
        {
            if (g_taskSelectedId == 0) {
                MessageBoxW(hWnd, L"삭제(비우기)할 과제를 선택해 주세요.", L"삭제", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }
            if (!CanDeleteOrDone()) {
                MessageBoxW(hWnd, L"팀장만 삭제할 수 있습니다.", L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TaskItem t = { 0 };
            t.id = g_taskSelectedId;
            t.done = 0;
            t.title[0] = 0;
            t.content[0] = 0;
            t.detail[0] = 0;
            t.file[0] = 0;

            if (!Task_Update(g_currentTeamId, &t)) {
                MessageBoxW(hWnd, L"삭제 실패(Task_Update)", L"삭제", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_ClearRightEdits();
            Task_SaveCurrentPageState();
            Task_RefreshLeftList();
            if (g_edTaDeadline) SetWindowTextW(g_edTaDeadline, L"");

            // ✅ 삭제 후 캘린더 즉시 반영
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

            MessageBoxW(hWnd, L"삭제 되었습니다.", L"삭제", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }


        // 파일 비우기
        if (HitScaled(R_TA_BTN_FILE_CLEAR_X1, R_TA_BTN_FILE_CLEAR_Y1, R_TA_BTN_FILE_CLEAR_X2, R_TA_BTN_FILE_CLEAR_Y2, x, y)) {
            if (g_edTaFile) SetWindowTextW(g_edTaFile, L"");
            SAFE_LEAVE();
        }

        // 조회
        if (HitScaled(R_TA_SEARCH_ICON_X1, R_TA_SEARCH_ICON_Y1, R_TA_SEARCH_ICON_X2, R_TA_SEARCH_ICON_Y2, x, y))
        {
            wchar_t key[128] = { 0 };
            if (g_edTaSearch) GetWindowTextW(g_edTaSearch, key, 128);

            if (!key[0]) {
                MessageBoxW(hWnd, L"조회 키워드를 입력해 주세요.", L"조회", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (!g_currentTeamId[0]) {
                MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요.", L"조회", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TaskItem found = { 0 };
            if (!Task_FindByTitle(g_currentTeamId, key, &found)) {
                MessageBoxW(hWnd, L"해당 키워드의 과제가 없습니다.", L"조회", MB_OK | MB_ICONINFORMATION);
                SAFE_LEAVE();
            }

            // ✅ 미완료 목록에서 found.id 위치 찾기
            TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
            int n = list ? Task_LoadActiveOnly(g_currentTeamId, list, 512) : 0;

            int pos = -1;
            for (int i = 0; i < n; i++) {
                if (list[i].id == found.id) { pos = i; break; }
            }

            if (pos >= 0) {
                g_taskPage = pos / 4;
                g_taskSelectedSlot = pos % 4;
                g_taskSelectedId = list[pos].id;
                Task_LoadToRightEdits(&list[pos]);

                Task_RefreshLeftList();   // 왼쪽 표시도 페이지에 맞게 갱신
                InvalidateRect(hWnd, NULL, FALSE);
            }
            else {
                // (이론상 거의 없지만) 완료로 넘어갔거나 목록에 없으면
                MessageBoxW(hWnd, L"미완료 목록에서 찾을 수 없습니다.", L"조회", MB_OK | MB_ICONINFORMATION);
            }

            if (list) free(list);
            SAFE_LEAVE();
        }

        // 등록
        if (HitScaled(R_TA_BTN_ADD_X1, R_TA_BTN_ADD_Y1, R_TA_BTN_ADD_X2, R_TA_BTN_ADD_Y2, x, y))
        {
            if (!g_currentTeamId[0]) {
                MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요.", L"등록", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TaskItem t = { 0 };
            t.done = 0;

            if (g_edTaTitle)   GetWindowTextW(g_edTaTitle, t.title, TASK_TITLE_MAX);
            if (g_edTaContent) GetWindowTextW(g_edTaContent, t.content, TASK_TEXT_MAX);
            if (g_edTaDetail)  GetWindowTextW(g_edTaDetail, t.detail, TASK_TEXT_MAX);
            if (g_edTaFile)    GetWindowTextW(g_edTaFile, t.file, TASK_FILE_MAX);
            if (g_edTaDeadline) GetWindowTextW(g_edTaDeadline, t.deadline, TASK_DEADLINE_MAX);


            if (!t.title[0]) {
                MessageBoxW(hWnd, L"제목을 입력해 주세요.", L"등록", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            // ✅ 빈 슬롯(id) 있으면 재사용해서 Update로 채움
            int reuseId = Task_FindFirstEmptyId(g_currentTeamId);
            if (reuseId != 0) {
                t.id = reuseId;

                if (!Task_Update(g_currentTeamId, &t)) {
                    MessageBoxW(hWnd, L"등록 실패(빈슬롯 Update 실패)", L"등록", MB_OK | MB_ICONERROR);
                    SAFE_LEAVE();
                }
            }
            else {
                // ✅ 없으면 기존처럼 Add
                if (!Task_Add(g_currentTeamId, &t)) {
                    MessageBoxW(hWnd, L"등록 실패(파일 저장 오류)", L"등록", MB_OK | MB_ICONERROR);
                    SAFE_LEAVE();
                }
            }

            g_taskSelectedSlot = -1;
            g_taskSelectedId = 0;
            Task_ClearRightEdits();
            Task_SaveCurrentPageState();
            Task_RefreshLeftList();

            MessageBoxW(hWnd, L"등록 완료!", L"등록", MB_OK | MB_ICONINFORMATION);
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

            SAFE_LEAVE();
        }



        // 수정
        if (HitScaled(R_TA_BTN_EDIT_X1, R_TA_BTN_EDIT_Y1,
            R_TA_BTN_EDIT_X2, R_TA_BTN_EDIT_Y2, x, y))
        {
            if (g_taskSelectedId == 0) {
                MessageBoxW(hWnd, L"왼쪽 목록에서 과제를 먼저 선택해 주세요.", L"수정", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TaskItem t = { 0 };
            t.id = g_taskSelectedId;

            if (g_edTaTitle)   GetWindowTextW(g_edTaTitle, t.title, TASK_TITLE_MAX);
            if (g_edTaContent) GetWindowTextW(g_edTaContent, t.content, TASK_TEXT_MAX);
            if (g_edTaDetail)  GetWindowTextW(g_edTaDetail, t.detail, TASK_TEXT_MAX);
            if (g_edTaFile)    GetWindowTextW(g_edTaFile, t.file, TASK_FILE_MAX);
            if (g_edTaDeadline)
                GetWindowTextW(g_edTaDeadline, t.deadline, TASK_DEADLINE_MAX);
            if (g_edTaDeadline) GetWindowTextW(g_edTaDeadline, t.deadline, TASK_DEADLINE_MAX);


            if (!Task_Update(g_currentTeamId, &t)) {
                MessageBoxW(hWnd, L"수정 실패", L"수정", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_RefreshLeftList();
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);
            MessageBoxW(hWnd, L"수정 완료!", L"수정", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }


        // 완료
        if (HitScaled(R_TA_BTN_DONE_X1, R_TA_BTN_DONE_Y1, R_TA_BTN_DONE_X2, R_TA_BTN_DONE_Y2, x, y))
        {
            if (g_taskSelectedId == 0) {
                MessageBoxW(hWnd, L"완료할 과제를 선택해 주세요.", L"완료", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }
            if (!CanDeleteOrDone()) {
                MessageBoxW(hWnd, L"팀장만 완료 처리할 수 있습니다.", L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }


            if (!Task_SetDone(g_currentTeamId, g_taskSelectedId, 1)) {
                MessageBoxW(hWnd, L"완료 처리 실패", L"완료", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            g_taskSelectedSlot = -1;
            Task_ClearRightEdits();
            Task_SaveCurrentPageState();
            Done_RefreshUI();
            Calendar_SetTeamId(g_currentTeamId);
            Calendar_NotifyTasksChanged(hWnd, g_currentTeamId);

            Task_RefreshLeftList();
            MessageBoxW(hWnd, L"완료 처리!", L"완료", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }


        // 페이지 이전
        if (HitScaled(R_TA_PAGE_PREV_X1, R_TA_PAGE_PREV_Y1, R_TA_PAGE_PREV_X2, R_TA_PAGE_PREV_Y2, x, y))
        {
            Task_SaveCurrentPageState();
            if (g_taskPage > 0) g_taskPage--;
            g_taskSelectedSlot = -1;
            g_taskSelectedId = 0;

            Task_RefreshLeftList();
            Task_RestorePageStateForPage(g_taskPage);

            InvalidateRect(hWnd, NULL, FALSE);
            SAFE_LEAVE();
        }

        // 페이지 다음
        if (HitScaled(R_TA_PAGE_NEXT_X1, R_TA_PAGE_NEXT_Y1, R_TA_PAGE_NEXT_X2, R_TA_PAGE_NEXT_Y2, x, y))
        {
            Task_SaveCurrentPageState();

            TaskItem* tmp = (TaskItem*)calloc(512, sizeof(TaskItem));
            int n = tmp ? Task_LoadActiveOnly(g_currentTeamId, tmp, 512) : 0;
            if (tmp) free(tmp);

            int maxPage = (n <= 0) ? 0 : ((n - 1) / 4);
            if (g_taskPage < maxPage) g_taskPage++;


            g_taskSelectedSlot = -1;
            g_taskSelectedId = 0;

            Task_RefreshLeftList();
            Task_RestorePageStateForPage(g_taskPage);

            InvalidateRect(hWnd, NULL, FALSE);
            SAFE_LEAVE();
        }

        // ✅ 파일칸 클릭 -> 파일 선택창 띄우기 (편의기능: 로컬경로 입력됨)
        if (HitScaled(R_TA_FILE_X1, R_TA_FILE_Y1, R_TA_FILE_X2, R_TA_FILE_Y2, x, y))
        {
            wchar_t picked[TASK_FILE_MAX] = { 0 };
            if (SelectFileDialog(hWnd, picked, TASK_FILE_MAX)) {
                // 로컬 경로가 들어감 (공유는 안 됨)
                if (g_edTaFile) SetWindowTextW(g_edTaFile, picked);
            }
            SAFE_LEAVE();
        }


        // 다운로드(열기) - 링크면 브라우저로, 로컬경로면 해당 PC에서 파일 열기
        if (HitScaled(R_TA_BTN_DOWNLOAD_X1, R_TA_BTN_DOWNLOAD_Y1,
            R_TA_BTN_DOWNLOAD_X2, R_TA_BTN_DOWNLOAD_Y2, x, y))
        {
            wchar_t path[TASK_FILE_MAX] = { 0 };
            if (g_edTaFile) GetWindowTextW(g_edTaFile, path, TASK_FILE_MAX);

            if (!path[0]) {
                MessageBoxW(hWnd, L"파일/링크가 없습니다.", L"다운로드", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            // ✅ http/https 링크면: 다른 사용자도 열 수 있음
            if (wcsncmp(path, L"http://", 7) == 0 || wcsncmp(path, L"https://", 8) == 0) {
                ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
                SAFE_LEAVE();
            }

            // ✅ 로컬 경로면: 그 PC에 파일이 있을 때만 열림
            ShellExecuteW(NULL, L"open", path, NULL, NULL, SW_SHOWNORMAL);
            SAFE_LEAVE();
        }


        SAFE_LEAVE();
    }


    if (g_screen == SCR_BOARD_WRITE)
    {
        // ✅ 등록 버튼 좌표(임시) → 너가 눌러서 좌표 맞춰야 함
        if (HitScaled(
            R_BDW_SAVE_X1, R_BDW_SAVE_Y1,
            R_BDW_SAVE_X2, R_BDW_SAVE_Y2,
            x, y))

        {
            wchar_t title[128] = { 0 };
            wchar_t content[2048] = { 0 };

            if (g_edBwTitle)   GetWindowTextW(g_edBwTitle, title, 128);
            if (g_edBwContent) GetWindowTextW(g_edBwContent, content, 2048);

            // ✅ 수정/새글 분기
            if (g_boardEditPostId > 0) {
                if (!Board_UpdatePost(g_boardEditPostId, title, content)) {
                    MessageBoxW(hWnd, L"수정 실패(파일 저장 오류)", L"게시판", MB_OK | MB_ICONERROR);
                    SAFE_LEAVE();
                }
                g_boardEditPostId = 0;
                MessageBoxW(hWnd, L"수정 되었습니다.", L"게시판", MB_OK | MB_ICONINFORMATION);
            }
            else {
                if (!Board_AddPost(title, g_currentUserId, content)) {
                    MessageBoxW(hWnd, L"등록 실패(파일 저장 오류)", L"게시판", MB_OK | MB_ICONERROR);
                    SAFE_LEAVE();
                }
                MessageBoxW(hWnd, L"등록 되었습니다.", L"게시판", MB_OK | MB_ICONINFORMATION);
            }

            // ✅ 게시판 리스트로 복귀
            SwitchScreen(hWnd, SCR_BOARD);
            SAFE_LEAVE();
        }

        SAFE_LEAVE();
    }


    // -----------------------------------------------------
// BOARD
// -----------------------------------------------------

    if (g_screen == SCR_BOARD)
    {
        // ✅ 조회(돋보기) 버튼
        if (HitScaled(R_BD_SEARCH_BTN_X1, R_BD_SEARCH_BTN_Y1,
            R_BD_SEARCH_BTN_X2, R_BD_SEARCH_BTN_Y2, x, y))
        {
            wchar_t q[128] = { 0 };
            if (g_edBdSearch) GetWindowTextW(g_edBdSearch, q, 128);

            // 공백만 있으면 조회 해제
            if (q[0] == 0) {
                Board_ClearSearch();
            }
            else {
                Board_SetSearchQuery(q);
                Board_ApplySearch();
            }

            InvalidateRect(hWnd, NULL, FALSE);
            SAFE_LEAVE();
        }



        // ✅ 등록 버튼 -> 글쓰기 화면 전환
        if (HitScaled(R_BOARD_BTN_REG_X1, R_BOARD_BTN_REG_Y1,
            R_BOARD_BTN_REG_X2, R_BOARD_BTN_REG_Y2, x, y))
        {
            if (g_currentTeamId[0] == 0) {
                MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요.", L"알림", MB_OK | MB_ICONINFORMATION);
                SAFE_LEAVE();
            }

            Board_SetTeamId(g_currentTeamId);
            g_boardEditPostId = 0;            // ✅ 새 글 모드
            SwitchScreen(hWnd, SCR_BOARD_WRITE);
            SAFE_LEAVE();
        }

        // ✅ 수정 버튼
        if (HitScaled(R_BDW_EDIT_X1, R_BDW_EDIT_Y1,
            R_BDW_EDIT_X2, R_BDW_EDIT_Y2, x, y))
        {
            int postId = Board_GetSelectedPostId();
            if (postId <= 0) {
                MessageBoxW(hWnd, L"수정할 게시물을 선택해 주세요.", L"수정", MB_OK | MB_ICONINFORMATION);
                SAFE_LEAVE();
            }

            wchar_t t[256] = L"", a[256] = L"", c[4096] = L"";
            if (!Board_GetPostById(postId, t, 256, a, 256, c, 4096)) {
                MessageBoxW(hWnd, L"게시물을 찾을 수 없습니다.", L"수정", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            // ✅ 글쓴이만 수정 가능
            if (lstrcmpW(a, g_currentUserId) != 0) {
                MessageBoxW(hWnd, L"글쓴이만 수정할 수 있습니다.", L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            g_boardEditPostId = postId;       // ✅ 수정 모드
            SwitchScreen(hWnd, SCR_BOARD_WRITE);
            SAFE_LEAVE();
        }

        // ✅ 삭제 버튼
        if (HitScaled(R_BDW_DEL_X1, R_BDW_DEL_Y1,
            R_BDW_DEL_X2, R_BDW_DEL_Y2, x, y))
        {
            int postId = Board_GetSelectedPostId();
            if (postId <= 0) {
                MessageBoxW(hWnd, L"삭제할 게시물을 선택해 주세요.", L"삭제", MB_OK | MB_ICONINFORMATION);
                SAFE_LEAVE();
            }

            wchar_t t[256] = L"", a[256] = L"", c[64] = L"";
            if (!Board_GetPostById(postId, t, 256, a, 256, c, 64)) {
                MessageBoxW(hWnd, L"게시물을 찾을 수 없습니다.", L"삭제", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            if (lstrcmpW(a, g_currentUserId) != 0) {
                MessageBoxW(hWnd, L"글쓴이만 삭제할 수 있습니다.", L"권한 없음", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (MessageBoxW(hWnd, L"정말 삭제할까요?", L"삭제", MB_YESNO | MB_ICONQUESTION) != IDYES)
                SAFE_LEAVE();

            if (!Board_DeletePost(postId)) {
                MessageBoxW(hWnd, L"삭제 실패", L"삭제", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            MessageBoxW(hWnd, L"삭제되었습니다.", L"삭제", MB_OK | MB_ICONINFORMATION);
            InvalidateRect(hWnd, NULL, FALSE);
            SAFE_LEAVE();
        }

        // ✅ 목록/페이지 클릭
        if (Board_OnClick(hWnd, x, y)) SAFE_LEAVE();

        SAFE_LEAVE();
    }
}


static int ScreenShowsCalendar(Screen s)
{
    return (s == SCR_MAIN ||
        s == SCR_DEADLINE ||
        s == SCR_TODO ||
        s == SCR_MYTEAM ||
        s == SCR_MYTEAM_DETAIL ||
        s == SCR_DONE ||
        s == SCR_TEAM_CREATE ||
        s == SCR_TEAM_JOIN);
}

// ---------------------------------------------------------
// Paint  ✅ 캘린더: ScreenShowsCalendar()인 화면에서만 표시
// ---------------------------------------------------------
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

    if (g_screen == SCR_START)                 DrawBitmapFit(mem, g_bmpStart, w, h);
    else if (g_screen == SCR_SIGNUP)           DrawBitmapFit(mem, g_bmpSignup, w, h);
    else if (g_screen == SCR_FINDPW)           DrawBitmapFit(mem, g_bmpFindPw, w, h);
    else if (g_screen == SCR_DEADLINE)         DrawBitmapFit(mem, g_bmpDeadline, w, h);
    else if (g_screen == SCR_TODO)             DrawBitmapFit(mem, g_bmpTodo, w, h);
    else if (g_screen == SCR_MYTEAM)           DrawBitmapFit(mem, g_bmpMyTeam, w, h);
    else if (g_screen == SCR_MYTEAM_DETAIL)    DrawBitmapFit(mem, g_bmpMyTeamDetail, w, h);
    else if (g_screen == SCR_DONE)             DrawBitmapFit(mem, g_bmpDone, w, h);
    else if (g_screen == SCR_TEAM_CREATE)      DrawBitmapFit(mem, g_bmpTeamCreate, w, h);
    else if (g_screen == SCR_TEAM_JOIN)        DrawBitmapFit(mem, g_bmpTeamJoin, w, h);
    else if (g_screen == SCR_TASK_ADD)         DrawBitmapFit(mem, g_bmpTaskAdd, w, h);
    else if (g_screen == SCR_BOARD)            DrawBitmapFit(mem, g_bmpBoard, w, h);
    else if (g_screen == SCR_BOARD_WRITE)      DrawBitmapFit(mem, g_bmpBoardWrite, w, h);
    else                                       DrawBitmapFit(mem, g_bmpMain, w, h);

    // ✅ 캘린더(필요한 화면에서만)
    if (ScreenShowsCalendar(g_screen))
    {
        ApplyCalendarClipForScreen(g_screen);
        Calendar_Draw(mem);
    }

    // ✅ MYTEAM: 선택 테두리
    if (g_screen == SCR_MYTEAM)
    {
        if (g_myTeamSelected >= 0 && g_myTeamSelected < MYTEAM_SLOT_MAX &&
            g_stMyTeam[g_myTeamSelected] && g_myTeams[g_myTeamSelected].team[0] != 0)
        {
            RECT r;
            GetWindowRect(g_stMyTeam[g_myTeamSelected], &r);

            POINT p1 = { r.left, r.top };
            POINT p2 = { r.right, r.bottom };
            ScreenToClient(hWnd, &p1);
            ScreenToClient(hWnd, &p2);

            RECT rcSel = { p1.x, p1.y, p2.x, p2.y };

            HPEN pen2 = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            HGDIOBJ oldPen = SelectObject(mem, pen2);
            HGDIOBJ oldBrush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));

            Rectangle(mem, rcSel.left - 2, rcSel.top - 2, rcSel.right + 2, rcSel.bottom + 2);

            SelectObject(mem, oldBrush);
            SelectObject(mem, oldPen);
            DeleteObject(pen2);
        }
    }

    // ✅ BOARD
    if (g_screen == SCR_BOARD)
    {
        Board_Draw(mem);
    }

    // ✅ TASK_ADD 선택 테두리
    if (g_screen == SCR_TASK_ADD)
    {
        RECT slots[4] = {
            MakeRcScaled(R_TA_ITEM1_X1, R_TA_ITEM1_Y1, R_TA_ITEM1_X2, R_TA_ITEM1_Y2),
            MakeRcScaled(R_TA_ITEM2_X1, R_TA_ITEM2_Y1, R_TA_ITEM2_X2, R_TA_ITEM2_Y2),
            MakeRcScaled(R_TA_ITEM3_X1, R_TA_ITEM3_Y1, R_TA_ITEM3_X2, R_TA_ITEM3_Y2),
            MakeRcScaled(R_TA_ITEM4_X1, R_TA_ITEM4_Y1, R_TA_ITEM4_X2, R_TA_ITEM4_Y2),
        };

        HPEN penThin = CreatePen(PS_SOLID, 1, RGB(180, 200, 215));
        HGDIOBJ oldPen = SelectObject(mem, penThin);
        HGDIOBJ oldBrush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));

        for (int i = 0; i < 4; i++) {
            Rectangle(mem, slots[i].left, slots[i].top, slots[i].right, slots[i].bottom);
        }

        SelectObject(mem, oldBrush);
        SelectObject(mem, oldPen);
        DeleteObject(penThin);

        if (g_taskSelectedSlot >= 0 && g_taskSelectedSlot < 4) {
            RECT r2 = slots[g_taskSelectedSlot];
            HPEN penBold = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            oldPen = SelectObject(mem, penBold);
            oldBrush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));

            Rectangle(mem, r2.left - 2, r2.top - 2, r2.right + 2, r2.bottom + 2);

            SelectObject(mem, oldBrush);
            SelectObject(mem, oldPen);
            DeleteObject(penBold);
        }
    }

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

    if (g_uiFont) { DeleteObject(g_uiFont); g_uiFont = NULL; }
    if (g_brWhite) { DeleteObject(g_brWhite); g_brWhite = NULL; }
    if (g_bmpBoardWrite) { DeleteObject(g_bmpBoardWrite); g_bmpBoardWrite = NULL; }

}




// ✅ TASK_ADD에서만 쓸 "미완료 목록" 로더 (done==0 && title 존재)
static int Task_LoadActiveOnly(const wchar_t* teamId, TaskItem* out, int cap)
{
    if (!teamId || !teamId[0] || !out || cap <= 0) return 0;

    TaskItem* all = (TaskItem*)calloc(512, sizeof(TaskItem));
    if (!all) return 0;

    int nAll = Task_LoadAll(teamId, all, 512);

    int k = 0;
    for (int i = 0; i < nAll && k < cap; i++) {
        if (all[i].title[0] == 0) continue;   // 비어있는 슬롯(삭제) 제외
        if (all[i].done == 1) continue;       // ✅ 완료된 과제 제외
        out[k++] = all[i];
    }

    free(all);
    return k; // 미완료 개수
}





// ---------------------------------------------------------
// MYTEAM 슬롯 배치
// ---------------------------------------------------------
static void LayoutMyTeamStatics(void)
{
    int x1 = SX(R_MYTEAM_LIST_X1);
    int y1 = SY(R_MYTEAM_LIST_Y1);
    int x2 = SX(R_MYTEAM_LIST_X2);
    int y2 = SY(R_MYTEAM_LIST_Y2);

    int w = x2 - x1;
    int h = y2 - y1;
    if (w <= 0 || h <= 0) return;

    int slotH = h / MYTEAM_SLOT_MAX;
    int padX = 6;
    int padY = 6;

    for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
    {
        if (!g_stMyTeam[i]) continue;

        int left = x1 + padX;
        int top = y1 + i * slotH + padY;
        int width = w - padX * 2;
        int height = slotH - padY * 2;

        MoveWindow(g_stMyTeam[i], left, top, width, height, TRUE);
    }
}
// ✅ 삭제된(비어있는) 과제 슬롯의 id를 찾아서 재사용하기
// - 삭제 로직이 title[0]=0 으로 "비우기" 하니까 그걸 빈 슬롯으로 간주
// - 가장 앞(작은 id)을 반환해서 "1번부터" 다시 채워지게 함
static int Task_FindFirstEmptyId(const wchar_t* teamId)
{
    if (!teamId || !teamId[0]) return 0;

    TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
    if (!list) return 0;

    int n = Task_LoadAll(teamId, list, 512);
    int best = 0;

    for (int i = 0; i < n; i++)
    {
        // ✅ 빈 슬롯 기준: title이 비어있음(삭제/비우기 된 상태)
        if (list[i].title[0] == 0)
        {
            // id가 0이면 이상데이터라 skip
            if (list[i].id <= 0) continue;

            // 가장 작은 id를 재사용 (1번 자리부터 다시 채워짐)
            if (best == 0 || list[i].id < best) best = list[i].id;
        }
    }

    free(list);
    return best;  // 0이면 재사용할 빈 슬롯 없음
}

static int TaskOwner_Get(const wchar_t* teamId, int taskId, wchar_t* outOwner, int outLen)
{
    if (!teamId || !teamId[0] || taskId <= 0 || !outOwner || outLen <= 0) return 0;
    outOwner[0] = 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, TASK_OWNERS_FILE, L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        wchar_t tid[64] = { 0 }, uid[128] = { 0 };
        int id = 0;
        // teamId|taskId|userId
        if (swscanf(line, L"%63[^|]|%d|%127[^|\r\n]", tid, &id, uid) != 3) continue;
        if (wcscmp(tid, teamId) == 0 && id == taskId) {
            lstrcpynW(outOwner, uid, outLen);
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static int TaskOwner_Set(const wchar_t* teamId, int taskId, const wchar_t* ownerId)
{
    if (!teamId || !teamId[0] || taskId <= 0 || !ownerId || !ownerId[0]) return 0;

    // 간단하게: 전체 읽고 해당 레코드 있으면 교체, 없으면 추가 후 재작성
    FILE* fp = NULL;
    _wfopen_s(&fp, TASK_OWNERS_FILE, L"r, ccs=UTF-8");

    wchar_t* lines = NULL;
    int count = 0, cap = 0;

    if (fp) {
        wchar_t buf[512];
        while (fgetws(buf, 512, fp)) {
            if (count >= cap) {
                cap = (cap == 0) ? 64 : cap * 2;
                lines = (wchar_t*)realloc(lines, sizeof(wchar_t) * cap * 512);
                if (!lines) { fclose(fp); return 0; }
            }
            // 한 줄 512 wchar_t로 저장
            wcsncpy(&lines[count * 512], buf, 511);
            lines[count * 512 + 511] = 0;
            count++;
        }
        fclose(fp);
    }

    int found = 0;
    for (int i = 0; i < count; i++) {
        wchar_t* line = &lines[i * 512];
        wchar_t tid[64] = { 0 }, uid[128] = { 0 };
        int id = 0;
        if (swscanf(line, L"%63[^|]|%d|%127[^|\r\n]", tid, &id, uid) != 3) continue;
        if (wcscmp(tid, teamId) == 0 && id == taskId) {
            swprintf(line, 512, L"%s|%d|%s\n", teamId, taskId, ownerId);
            found = 1;
            break;
        }
    }

    _wfopen_s(&fp, TASK_OWNERS_FILE, L"w, ccs=UTF-8");
    if (!fp) { if (lines) free(lines); return 0; }

    for (int i = 0; i < count; i++) {
        fputws(&lines[i * 512], fp);
    }
    if (!found) {
        fwprintf(fp, L"%s|%d|%s\n", teamId, taskId, ownerId);
    }
    fclose(fp);

    if (lines) free(lines);
    return 1;
}

static int TaskOwner_IsMe(const wchar_t* teamId, int taskId)
{
    wchar_t owner[128] = { 0 };
    if (!TaskOwner_Get(teamId, taskId, owner, 128)) return 0;
    return (wcscmp(owner, g_currentUserId) == 0);
}








// ---------------------------------------------------------
// TASK_ADD helper
// ---------------------------------------------------------
static void Task_ClearRightEdits(void)
{
    if (g_edTaTitle)   SetWindowTextW(g_edTaTitle, L"");
    if (g_edTaContent) SetWindowTextW(g_edTaContent, L"");
    if (g_edTaDetail)  SetWindowTextW(g_edTaDetail, L"");
    if (g_edTaFile)    SetWindowTextW(g_edTaFile, L"");
    if (g_edTaDeadline) SetWindowTextW(g_edTaDeadline, L"");

    g_taskSelectedId = 0;
}

static void Task_LoadToRightEdits(const TaskItem* t)
{
    if (!t) return;

    if (g_edTaTitle)   SetWindowTextW(g_edTaTitle, t->title);
    if (g_edTaContent) SetWindowTextW(g_edTaContent, t->content);
    if (g_edTaDetail)  SetWindowTextW(g_edTaDetail, t->detail);
    if (g_edTaFile)    SetWindowTextW(g_edTaFile, t->file);
    if (g_edTaDeadline) SetWindowTextW(g_edTaDeadline, t->deadline);

    g_taskSelectedId = t->id;
}

static void Task_RefreshLeftList(void)
{
    if (!g_currentTeamId[0]) {
        if (g_edTaTask1) SetWindowTextW(g_edTaTask1, L"1.");
        if (g_edTaTask2) SetWindowTextW(g_edTaTask2, L"2.");
        if (g_edTaTask3) SetWindowTextW(g_edTaTask3, L"3.");
        if (g_edTaTask4) SetWindowTextW(g_edTaTask4, L"4.");
        return;
    }

    TaskItem* buf = (TaskItem*)calloc(512, sizeof(TaskItem));   // ✅ 힙
    if (!buf) return;

    int n = Task_LoadActiveOnly(g_currentTeamId, buf, 512);      // ✅ 압축 목록
    int base = g_taskPage * 4;

    for (int i = 0; i < 4; i++) {
        int idx = base + i;

        wchar_t line[160] = L"";
        if (idx < n) swprintf(line, 160, L"%d. %ls", idx + 1, buf[idx].title);
        else         swprintf(line, 160, L"%d.", idx + 1);

        if (i == 0 && g_edTaTask1) SetWindowTextW(g_edTaTask1, line);
        if (i == 1 && g_edTaTask2) SetWindowTextW(g_edTaTask2, line);
        if (i == 2 && g_edTaTask3) SetWindowTextW(g_edTaTask3, line);
        if (i == 3 && g_edTaTask4) SetWindowTextW(g_edTaTask4, line);
    }

    free(buf);
}


// ---------------------------------------------------------
// 디버그 오버레이
// ---------------------------------------------------------
static void DrawDebugOverlay(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    HFONT f = CreateFontW(
        18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HFONT old = (HFONT)SelectObject(hdc, f);

    wchar_t buf2[128];
    swprintf(buf2, 128, L"User:%s Team:%s Role:%s", g_currentUserId, g_currentTeamId, g_currentRole);
    TextOutW(hdc, 20, 10, buf2, (int)wcslen(buf2));   // ✅ 더 위로

    wchar_t buf3[128];
    swprintf(buf3, 128, L"Last Click: (%d, %d)", g_lastX, g_lastY);
    TextOutW(hdc, 20, 30, buf3, (int)wcslen(buf3));   // ✅ 좌표 복구

    SelectObject(hdc, old);
    DeleteObject(f);
}
// ---------------------------------------------------------
// 링크 에러 방지용
// ---------------------------------------------------------

void App_GoToStart(HWND hWnd) { SwitchScreen(hWnd, SCR_START); }
int App_OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* dis)
{
    (void)hWnd; (void)dis;
    return 0;
}

LRESULT App_OnDrawItemWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
    if (dis) {
        if (App_OnDrawItem(hWnd, dis)) return TRUE;
    }
    return FALSE;
}

LRESULT App_OnMouseMoveWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    (void)hWnd; (void)wParam; (void)lParam;
    return 0;
}

LRESULT App_OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    return App_OnMouseMoveWndProc(hWnd, wParam, lParam);
}

// ---------------------------------------------------------
// 외부에서 호출되는 함수들
// ---------------------------------------------------------
void RefreshMyTeamList(HWND hWnd)
{
    MyTeam_RefreshUI(hWnd);
}

void SwitchToTeam(HWND hWnd, const wchar_t* teamId)
{
    if (!teamId || !teamId[0])
        return;

    lstrcpynW(g_currentTeamId, teamId, 64);

    // 🔴 여기 추가
    Board_SetTeamId(g_currentTeamId);

    // 캘린더도 같이 바꾸는 게 좋음
    Calendar_SetTeamId(g_currentTeamId);
    Calendar_RebuildFromTasks(g_currentTeamId);

    InvalidateRect(hWnd, NULL, TRUE);
}

static int ParseDateToSystemTime_YYYYMMDD(const wchar_t* s, SYSTEMTIME* out)
{
    if (!s || !s[0] || !out) return 0;

    int y = 0, m = 0, d = 0;
    if (swscanf(s, L"%d/%d/%d", &y, &m, &d) != 3) return 0;
    if (y < 1900 || m < 1 || m > 12 || d < 1 || d > 31) return 0;

    ZeroMemory(out, sizeof(*out));
    out->wYear = (WORD)y;
    out->wMonth = (WORD)m;
    out->wDay = (WORD)d;
    return 1;
}


static int IsDueSoon(const wchar_t* deadline, int days) // days: 예) 3일 이내
{
    SYSTEMTIME stDue, stNow;
    FILETIME ftDue, ftNow;

    if (!ParseDateToSystemTime_YYYYMMDD(deadline, &stDue)) return 0;  // ✅


    GetLocalTime(&stNow);

    // 날짜 비교를 위해 FILETIME 변환
    if (!SystemTimeToFileTime(&stDue, &ftDue)) return 0;
    if (!SystemTimeToFileTime(&stNow, &ftNow)) return 0;

    ULONGLONG due = (((ULONGLONG)ftDue.dwHighDateTime) << 32) | ftDue.dwLowDateTime;
    ULONGLONG now = (((ULONGLONG)ftNow.dwHighDateTime) << 32) | ftNow.dwLowDateTime;

    // 이미 지난 마감은 “임박”에서 제외(원하면 포함으로 바꿔도 됨)
    if (due < now) return 0;

    // FILETIME 단위: 100ns
    ULONGLONG diff100ns = due - now;
    ULONGLONG diffDays = diff100ns / (10000000ULL * 60 * 60 * 24);

    return (diffDays <= (ULONGLONG)days);
}


static int ParseDate_YYYYMMDD(const wchar_t* s, int* y, int* m, int* d)
{
    if (!s || !s[0]) return 0;
    int yy = 0, mm = 0, dd = 0;
    if (swscanf(s, L"%d/%d/%d", &yy, &mm, &dd) != 3) return 0;
    if (yy < 1900 || mm < 1 || mm > 12 || dd < 1 || dd > 31) return 0;
    *y = yy; *m = mm; *d = dd;
    return 1;
}

static int DaysUntil_YMD(int y, int m, int d)
{
    SYSTEMTIME stNow;
    GetLocalTime(&stNow);

    FILETIME ftNow, ftTarget;
    SYSTEMTIME stTarget = { 0 };
    stTarget.wYear = (WORD)y;
    stTarget.wMonth = (WORD)m;
    stTarget.wDay = (WORD)d;

    if (!SystemTimeToFileTime(&stNow, &ftNow)) return 999999;
    if (!SystemTimeToFileTime(&stTarget, &ftTarget)) return 999999;

    ULARGE_INTEGER a, b;
    a.LowPart = ftNow.dwLowDateTime;   a.HighPart = ftNow.dwHighDateTime;
    b.LowPart = ftTarget.dwLowDateTime; b.HighPart = ftTarget.dwHighDateTime;

    // 1 day = 86400 sec = 86400*10^7 (FILETIME tick = 100ns)
    const ULONGLONG DAY = 86400ULL * 10000000ULL;

    if (b.QuadPart < a.QuadPart) return -1; // 이미 지남
    ULONGLONG diff = b.QuadPart - a.QuadPart;
    return (int)(diff / DAY);
}

static void Main_RefreshBoxes(void)
{
    if (!g_edMainTodoList || !g_edMainUrgentList) return;

    if (!g_currentTeamId[0]) {
        SetWindowTextW(g_edMainTodoList, L"팀을 먼저 선택해 주세요.");
        SetWindowTextW(g_edMainUrgentList, L"팀을 먼저 선택해 주세요.");
        return;
    }

    TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
    int n = list ? Task_LoadActiveOnly(g_currentTeamId, list, 512) : 0;

    wchar_t todo[8192] = L"";
    wchar_t urg[8192] = L"";

    int tcnt = 0, ucnt = 0;

    for (int i = 0; i < n; i++) {
        // ✅ 미완료 박스: 제목만 쭉
        {
            wchar_t line[256];
            swprintf(line, 256, L"%d. %ls\r\n", ++tcnt, list[i].title);
            if (wcslen(todo) + wcslen(line) < 8190) wcscat(todo, line);
        }

        // ✅ 마감 임박 박스: 마감일 있고, D-10 이내만
        if (list[i].deadline[0]) {
            int d = DaysUntil_YYYYMMDD(list[i].deadline);
            if (d >= 0 && d <= 10) {
                wchar_t line2[256];
                swprintf(line2, 256, L"%d. %ls  (%ls, D-%d)\r\n",
                    ++ucnt, list[i].title, list[i].deadline, d);
                if (wcslen(urg) + wcslen(line2) < 8190) wcscat(urg, line2);
            }
        }
    }

    if (tcnt == 0) wcscpy(todo, L"(미완료 과제가 없습니다)");
    if (ucnt == 0) wcscpy(urg, L"(마감 임박(D-10) 과제가 없습니다)");

    SetWindowTextW(g_edMainTodoList, todo);
    SetWindowTextW(g_edMainUrgentList, urg);

    if (list) free(list);
}
static void Deadline_RefreshUI(void)
{
    if (!g_edOverlayList) return;

    if (!g_currentTeamId[0]) {
        SetWindowTextW(g_edOverlayList, L"팀을 먼저 선택해 주세요.");
        return;
    }

    TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
    int n = list ? Task_LoadActiveOnly(g_currentTeamId, list, 512) : 0;

    wchar_t out[8192];
    out[0] = 0;

    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!list[i].title[0]) continue;
        if (!list[i].deadline[0]) continue;

        int d = DaysUntil_YYYYMMDD(list[i].deadline);

        // ✅ D-10 이내만 보여주고 싶으면 아래 조건 유지
        if (d < 0 || d > 10) continue;

        wchar_t line[256];
        swprintf(line, 256, L"%d. %ls  (%ls, D-%d)\r\n",
            ++k, list[i].title, list[i].deadline, d);

        if (wcslen(out) + wcslen(line) < 8190) wcscat(out, line);
    }

    if (k == 0) wcscpy(out, L"(마감 임박 과제가 없습니다)");
    SetWindowTextW(g_edOverlayList, out);

    if (list) free(list);
}

static void Todo_RefreshUI(void)
{
    if (!g_edOverlayList) return;

    if (!g_currentTeamId[0]) {
        SetWindowTextW(g_edOverlayList, L"팀을 먼저 선택해 주세요.");
        return;
    }

    TaskItem* list = (TaskItem*)calloc(512, sizeof(TaskItem));
    int n = list ? Task_LoadActiveOnly(g_currentTeamId, list, 512) : 0;

    wchar_t out[8192];
    out[0] = 0;

    int k = 0;
    for (int i = 0; i < n; i++) {
        if (!list[i].title[0]) continue;

        wchar_t line[256];
        swprintf(line, 256, L"%d. %ls\r\n", ++k, list[i].title);

        if (wcslen(out) + wcslen(line) < 8190) wcscat(out, line);
    }

    if (k == 0) wcscpy(out, L"(미완료 과제가 없습니다)");
    SetWindowTextW(g_edOverlayList, out);

    if (list) free(list);
}

LRESULT App_OnAppChildClickWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    int x = (int)wParam;
    int y = (int)lParam;

    // 자식 EDIT/STATIC 클릭도 부모 클릭 처리로 흘려보내기
    App_OnLButtonDown(hWnd, x, y);
    return 0;
}

static void ApplyCalendarClipForScreen(Screen s)
{
    Calendar_SetClipMode(0);
    Calendar_SetClipX(0);

    switch (s)
    {
    case SCR_TEAM_CREATE:
    case SCR_TEAM_JOIN:
        // 일~수만 보이게
        Calendar_SetClipMode(2);
        break;

    case SCR_MYTEAM:
    case SCR_MYTEAM_DETAIL:
        // 목~토만 보이게
        Calendar_SetClipMode(3);
        break;

    case SCR_DEADLINE:
    case SCR_TODO:
    case SCR_DONE:
        // ✅ 마감/미완료/완료: 왼쪽 박스가 일월화 영역 덮음 → 수~토만 보이게
        Calendar_SetClipMode(1);
        break;

    default:
        Calendar_SetClipMode(0);
        break;
    }
}

// -----------------------------------------------------
// 팀장 자동 위임 (참여 순)
// - OWNER → MEMBER
// - 가장 먼저 가입한 MEMBER → OWNER
// -----------------------------------------------------
static int Members_TransferOwnerAuto(const wchar_t* teamId,
    const wchar_t* currentOwnerId)
{
    if (!teamId || !teamId[0]) return 0;

    TMRow* rows = NULL;
    int n = 0;

    if (!ReadAllTeamMembers(&rows, &n)) return 0;

    int nextIdx = -1;

    // 1️⃣ 같은 팀의 첫 MEMBER 찾기
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;

        if (wcscmp(rows[i].role, L"MEMBER") == 0) {
            nextIdx = i;
            break;
        }
    }

    if (nextIdx == -1) {
        if (rows) free(rows);
        return 0; // 위임할 사람 없음
    }

    // 2️⃣ 현재 OWNER → MEMBER로 변경
    for (int i = 0; i < n; i++) {
        if (wcscmp(rows[i].tid, teamId) != 0) continue;
        if (wcscmp(rows[i].uid, currentOwnerId) == 0) {
            lstrcpynW(rows[i].role, L"MEMBER", 32);
            break;
        }
    }

    // 3️⃣ 새 OWNER 지정
    lstrcpynW(rows[nextIdx].role, L"OWNER", 32);

    int ok = WriteAllTeamMembers(rows, n);
    free(rows);
    return ok;
}