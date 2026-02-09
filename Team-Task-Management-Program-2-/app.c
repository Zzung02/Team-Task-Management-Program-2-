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

// TASK_ADD helper
static void Task_LoadToRightEdits(const TaskItem* t);
static void Task_RefreshLeftList(void);
static void Task_ClearRightEdits(void);
static int Task_FindFirstEmptyId(const wchar_t* teamId);






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

typedef enum { OVR_NONE = 0, OVR_DEADLINE = 1 } Overlay;
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

// ---------------------------------------------------------
// MAIN 상단 텍스트 유지용
// ---------------------------------------------------------
static wchar_t g_mainTeamText[128] = L"";
static wchar_t g_mainTaskText[128] = L"";
static wchar_t g_mainCodeText[128] = L"";
static HWND g_stMainCode = NULL;
static HWND g_edDoneList = NULL;

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
#define WM_APP_CHILDCLICK (WM_APP + 101)   // app.h에 이미 있으면 여기선 빼도 됨

static LRESULT CALLBACK EditHookProc(HWND hEdit, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN) {
        HWND parent = GetParent(hEdit);

        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(parent, &pt);

        g_lastX = pt.x;
        g_lastY = pt.y;
        InvalidateRect(parent, NULL, FALSE);

        // ✅ 직접 호출 X -> 메시지로 우회
        PostMessageW(parent, WM_APP_CHILDCLICK, (WPARAM)pt.x, (LPARAM)pt.y);

        // ✅ 여기서 oldProc 계속 타게 두면 안전(화면전환은 부모에서 처리)
    }

    WNDPROC oldProc = (WNDPROC)GetPropW(hEdit, PROP_OLD_EDIT_PROC);
    if (!oldProc) return DefWindowProcW(hEdit, msg, wParam, lParam);
    return CallWindowProcW(oldProc, hEdit, msg, wParam, lParam);
}



static void HookEditClick(HWND hEdit)
{
    if (!hEdit) return;
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditHookProc);
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
        InvalidateRect(parent, NULL, FALSE);

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
    WNDPROC oldProc = (WNDPROC)SetWindowLongPtrW(hSt, GWLP_WNDPROC, (LONG_PTR)StaticHookProc);
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

static int  g_taskSelectedSlot = -1;
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
    if (g_edTaFile)    GetWindowTextW(g_edTaFile, t.file, TASK_FILE_MAX);

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
    }

    if (!userId || !userId[0]) return 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512];
    int count = 0;

    while (fgetws(line, 512, fp) && count < MYTEAM_SLOT_MAX)
    {
        wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        int m = swscanf(line, L"%31[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role);
        if (m != 3) continue;
        if (wcscmp(uid, userId) != 0) continue;

        int dup = 0;
        for (int k = 0; k < count; k++) {
            if (wcscmp(g_myTeams[k].teamId, tid) == 0) { dup = 1; break; }
        }
        if (dup) continue;

        TeamInfo t = { 0 };
        if (Team_FindByTeamId(tid, &t)) {
            // ... while loop 안에서 Team_FindByTeamId 성공했을 때
            lstrcpynW(g_myTeams[count].team, t.teamName, 128);
            lstrcpynW(g_myTeams[count].teamId, t.teamId, 64);
            lstrcpynW(g_myTeams[count].joinCode, t.joinCode, 64);
            lstrcpynW(g_myTeams[count].role, role, 32);   // ✅ 추가
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
        if (g_stMyTeam[i]) continue;

        g_stMyTeam[i] = CreateWindowExW(
            0, L"STATIC", L"",
            WS_CHILD | SS_LEFT,
            0, 0, 10, 10,
            hWnd,
            (HMENU)(INT_PTR)(6000 + i),
            GetModuleHandleW(NULL),
            NULL
        );

        if (font) SendMessageW(g_stMyTeam[i], WM_SETFONT, (WPARAM)font, TRUE);
        ShowWindow(g_stMyTeam[i], SW_HIDE);

        // 필요 시 클릭 훅 달고 싶으면 여기서 HookStaticClick(g_stMyTeam[i])도 가능
        // (현재는 MYTEAM 선택을 GetWindowRect 기반으로 처리 중이라 없어도 됨)
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
    case SCR_DONE:
    case SCR_TEAM_CREATE:
    case SCR_TEAM_JOIN:
        return 1;

    default:
        return 0;
    }
}

// ---------------------------------------------------------
// 모든 컨트롤 제거
// ---------------------------------------------------------
static void DestroyAllEdits(void)
{
    

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
        g_edFindResult = CreateEdit(hWnd, 303, ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
        break;

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
        g_edTaDetail = CreateEdit(hWnd, 908, 0);
        g_edTaFile = CreateEdit(hWnd, 909, 0);

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
        Board_CreateControls(hWnd);
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

    if (g_edDoneList) ShowWindow(g_edDoneList, SW_HIDE);

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

    // BOARD
    if (g_screen == SCR_BOARD) {
        Board_RelayoutControls(hWnd);
        return;
    }

    // MYTEAM
    if (g_screen == SCR_MYTEAM) {
        MyTeam_RefreshUI(hWnd);
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

    if (!g_bmpSignup || !g_bmpMain || !g_bmpFindPw ||
        !g_bmpDeadline || !g_bmpTodo || !g_bmpMyTeam || !g_bmpDone ||
        !g_bmpTeamCreate || !g_bmpTeamJoin || !g_bmpTaskAdd || !g_bmpBoard) {
        return -1;
    }

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
                lstrcpynW(g_currentUserId, id, 128);
                g_currentTeamId[0] = 0;
                g_mainTeamText[0] = 0;
                g_mainTaskText[0] = 0;
                g_mainCodeText[0] = 0;

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
        if (HitScaled(R_MAIN_BTN_DEADLINE_X1, R_MAIN_BTN_DEADLINE_Y1, R_MAIN_BTN_DEADLINE_X2, R_MAIN_BTN_DEADLINE_Y2, x, y)) { SwitchScreen(hWnd, SCR_DEADLINE); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_TODO_X1, R_MAIN_BTN_TODO_Y1, R_MAIN_BTN_TODO_X2, R_MAIN_BTN_TODO_Y2, x, y)) { SwitchScreen(hWnd, SCR_TODO); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_MYTEAM_X1, R_MAIN_BTN_MYTEAM_Y1, R_MAIN_BTN_MYTEAM_X2, R_MAIN_BTN_MYTEAM_Y2, x, y)) { SwitchScreen(hWnd, SCR_MYTEAM); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_DONE_X1, R_MAIN_BTN_DONE_Y1, R_MAIN_BTN_DONE_X2, R_MAIN_BTN_DONE_Y2, x, y)) { SwitchScreen(hWnd, SCR_DONE); SAFE_LEAVE(); }

        if (HitScaled(R_MAIN_BTN_TEAM_CREATE_X1, R_MAIN_BTN_TEAM_CREATE_Y1, R_MAIN_BTN_TEAM_CREATE_X2, R_MAIN_BTN_TEAM_CREATE_Y2, x, y)) { SwitchScreen(hWnd, SCR_TEAM_CREATE); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_TEAM_JOIN_X1, R_MAIN_BTN_TEAM_JOIN_Y1, R_MAIN_BTN_TEAM_JOIN_X2, R_MAIN_BTN_TEAM_JOIN_Y2, x, y)) { SwitchScreen(hWnd, SCR_TEAM_JOIN); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_TASK_ADD_X1, R_MAIN_BTN_TASK_ADD_Y1, R_MAIN_BTN_TASK_ADD_X2, R_MAIN_BTN_TASK_ADD_Y2, x, y)) { SwitchScreen(hWnd, SCR_TASK_ADD); SAFE_LEAVE(); }
        if (HitScaled(R_MAIN_BTN_BOARD_X1, R_MAIN_BTN_BOARD_Y1, R_MAIN_BTN_BOARD_X2, R_MAIN_BTN_BOARD_Y2, x, y)) { SwitchScreen(hWnd, SCR_BOARD); SAFE_LEAVE(); }

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
            lstrcpynW(g_currentRole, g_myTeams[g_myTeamSelected].role, 32); // ✅ 추가

            SwitchScreen(hWnd, SCR_MAIN);
            ApplyMainHeaderTextsReal();
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
    // ✅ 왼쪽 과제목록(1~4) 클릭 -> 선택 처리
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
                // 빈칸 클릭이면 선택 해제
                g_taskSelectedSlot = -1;
                g_taskSelectedId = 0;
                Task_ClearRightEdits();
                InvalidateRect(hWnd, NULL, FALSE);
            }

            if (list) free(list);
        }

        SAFE_LEAVE();
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

            if (!Task_Update(g_currentTeamId, &t)) {
                MessageBoxW(hWnd, L"수정 실패", L"수정", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_RefreshLeftList();
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


    // -----------------------------------------------------
    // BOARD
    // -----------------------------------------------------
    if (g_screen == SCR_BOARD) {
        if (Board_OnClick(hWnd, x, y)) SAFE_LEAVE();
        SAFE_LEAVE();
    }

    SAFE_LEAVE();
}

// ---------------------------------------------------------
// Paint
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

    // MYTEAM 테두리 표시
    if (g_screen == SCR_MYTEAM)
    {
        for (int i = 0; i < MYTEAM_SLOT_MAX; i++)
        {
            if (!g_stMyTeam[i]) continue;

            RECT r;
            GetWindowRect(g_stMyTeam[i], &r);

            POINT p1 = { r.left, r.top };
            POINT p2 = { r.right, r.bottom };
            ScreenToClient(hWnd, &p1);
            ScreenToClient(hWnd, &p2);

            RECT rcSlot = { p1.x, p1.y, p2.x, p2.y };

            HPEN pen1 = CreatePen(PS_SOLID, 1, RGB(180, 200, 215));
            HGDIOBJ oldPen = SelectObject(mem, pen1);
            HGDIOBJ oldBrush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));
            Rectangle(mem, rcSlot.left, rcSlot.top, rcSlot.right, rcSlot.bottom);
            SelectObject(mem, oldBrush);
            SelectObject(mem, oldPen);
            DeleteObject(pen1);
        }

        if (g_myTeamSelected >= 0 && g_myTeamSelected < MYTEAM_SLOT_MAX && g_stMyTeam[g_myTeamSelected])
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

    // TASK_ADD 선택 테두리
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
            RECT r = slots[g_taskSelectedSlot];
            HPEN penBold = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
            oldPen = SelectObject(mem, penBold);
            oldBrush = SelectObject(mem, GetStockObject(HOLLOW_BRUSH));
            Rectangle(mem, r.left - 2, r.top - 2, r.right + 2, r.bottom + 2);
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

    g_taskSelectedId = 0;
}

static void Task_LoadToRightEdits(const TaskItem* t)
{
    if (!t) return;

    if (g_edTaTitle)   SetWindowTextW(g_edTaTitle, t->title);
    if (g_edTaContent) SetWindowTextW(g_edTaContent, t->content);
    if (g_edTaDetail)  SetWindowTextW(g_edTaDetail, t->detail);
    if (g_edTaFile)    SetWindowTextW(g_edTaFile, t->file);

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
    if (!teamId || !teamId[0]) return;

    TeamInfo t = { 0 };
    if (!Team_FindByTeamId(teamId, &t)) return;

    lstrcpynW(g_currentTeamId, t.teamId, 64);
    lstrcpynW(g_mainTeamText, t.teamName, 128);

    g_mainTaskText[0] = 0;
    g_mainCodeText[0] = 0;

    SwitchScreen(hWnd, SCR_MAIN);
    ApplyMainHeaderTextsReal();
} 