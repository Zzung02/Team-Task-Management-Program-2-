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

static void LayoutMyTeamStatics(void);




// =========================================================
// Forward decl
// =========================================================
static void RelayoutControls(HWND hWnd);
static void ResizeToBitmap(HWND hWnd, HBITMAP bmp);
static void SwitchScreen(HWND hWnd, Screen next);
static void SwitchScreen_NoHistory(HWND hWnd, Screen next);
static void DrawDebugOverlay(HDC hdc);
static HFONT GetUIFont(void);



// → 아래 프로토타입 + 맨 아래 스텁(빈 구현)만 추가함.
int    App_OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* dis);
LRESULT App_OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam);

// (이미 아래에 구현돼 있음. 혹시 헤더에 없을 때 대비해서 프로토타입만)
LRESULT App_OnDrawItemWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT App_OnMouseMoveWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);

// =========================================================
// 전역 상태
// =========================================================
Screen g_screen = SCR_START;

typedef enum { OVR_NONE = 0, OVR_DEADLINE = 1 } Overlay;
static Overlay g_overlay = OVR_NONE;

// (오버레이 늘리면 여기 추가)
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
static wchar_t g_mainCodeText[128] = L"";
static HWND g_stMainCode = NULL;

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

        // ✅ App_OnLButtonDown 호출 금지(재귀/스택오버플로우 원인)
        // Last Click만 갱신
        g_lastX = pt.x;
        g_lastY = pt.y;
        InvalidateRect(parent, NULL, FALSE);
    }

    WNDPROC oldProc = (WNDPROC)GetPropW(hEdit, PROP_OLDPROC);
    if (!oldProc) return DefWindowProcW(hEdit, msg, wParam, lParam);
    return CallWindowProcW(oldProc, hEdit, msg, wParam, lParam);
}

// ✅ 외부(board.c)에서도 쓸 수 있도록 공개
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
    SetPropW(h, PROP_OLDPROC, (HANDLE)(UINT_PTR)oldProc);


    return h;
}

// 기존 CreateEdit 호출 유지
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

// TASK_ADD
static HWND g_edTaSearch = NULL;
static HWND g_edTaTask1 = NULL;
static HWND g_edTaTask2 = NULL;
static HWND g_edTaTask3 = NULL;
static HWND g_edTaTask4 = NULL;
static HWND g_edTaTitle = NULL;
static HWND g_edTaContent = NULL;  // 멀티라인
static HWND g_edTaDetail = NULL;   // 단일라인
static HWND g_edTaFile = NULL;     // 단일라인

static int g_taskSelectedId = 0;
static int g_taskPage = 0;


// =========================================================
// MYTEAM: 5칸 STATIC 슬롯 (ListBox 완전 대체)
// =========================================================
#define MYTEAM_SLOT_MAX 8
static HWND    g_stMyTeam[MYTEAM_SLOT_MAX] = { 0 };
typedef struct {
    wchar_t team[128];
    wchar_t task[128];
    wchar_t teamId[64];
    wchar_t joinCode[64];
} MyTeamInfo;

static MyTeamInfo g_myTeams[MYTEAM_SLOT_MAX];



static int g_myTeamSelected = -1;

// teams.txt: (둘 다 지원)
// 1) team|task|code
// 2) userId|team|task|code
static int LoadMyTeams_FromMembers(const wchar_t* userId)
{
    // 초기화
    for (int i = 0; i < MYTEAM_SLOT_MAX; i++) {
        g_myTeams[i].team[0] = 0;
        g_myTeams[i].task[0] = 0;
        g_myTeams[i].teamId[0] = 0;
        g_myTeams[i].joinCode[0] = 0;

    }

    if (!userId || !userId[0]) return 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) {
        // 파일 없으면 팀 없음
        return 0;
    }

    wchar_t line[512];
    int count = 0;

    while (fgetws(line, 512, fp) && count < MYTEAM_SLOT_MAX)
    {
        // tid|uid|role
        wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        int m = swscanf(line, L"%31[^|]|%127[^|]|%31[^|\r\n]", tid, uid, role);
        if (m != 3) continue;

        if (wcscmp(uid, userId) != 0) continue;

        // 중복 teamId 방지
        int dup = 0;
        for (int k = 0; k < count; k++) {
            if (wcscmp(g_myTeams[k].teamId, tid) == 0) { // code칸을 임시로 teamId 저장해도 되고
                dup = 1; break;
            }
        }
        if (dup) continue;

        // teamId로 팀정보 조회
        TeamInfo t = { 0 };
        if (Team_FindByTeamId(tid, &t)) {
            lstrcpynW(g_myTeams[count].team, t.teamName, 128);
            lstrcpynW(g_myTeams[count].task, t.taskName, 128);
            lstrcpynW(g_myTeams[count].teamId, t.teamId, 64);
            lstrcpynW(g_myTeams[count].joinCode, t.joinCode, 64);
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
            swprintf(buf, 260, L"%s   [코드:%s]",
                g_myTeams[i].team, g_myTeams[i].joinCode);


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
    (void)hWnd;
    EnsureMyTeamStatics(hWnd, GetUIFont());
    LoadMyTeams_FromMembers(g_currentUserId);
    ApplyMyTeamTextsToUI();
    LayoutMyTeamStatics();
    ShowMyTeamStatics(1);
}

// =========================================================
// [DO NOT TOUCH] 모든 Edit 제거
// =========================================================
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

    if (g_edTaSearch) { RemovePropW(g_edTaSearch, PROP_OLDPROC); DestroyWindow(g_edTaSearch); g_edTaSearch = NULL; }
    if (g_edTaTask1) { RemovePropW(g_edTaTask1, PROP_OLDPROC); DestroyWindow(g_edTaTask1); g_edTaTask1 = NULL; }
    if (g_edTaTask2) { RemovePropW(g_edTaTask2, PROP_OLDPROC); DestroyWindow(g_edTaTask2); g_edTaTask2 = NULL; }
    if (g_edTaTask3) { RemovePropW(g_edTaTask3, PROP_OLDPROC); DestroyWindow(g_edTaTask3); g_edTaTask3 = NULL; }
    if (g_edTaTask4) { RemovePropW(g_edTaTask4, PROP_OLDPROC); DestroyWindow(g_edTaTask4); g_edTaTask4 = NULL; }
    if (g_edTaTitle) { RemovePropW(g_edTaTitle, PROP_OLDPROC); DestroyWindow(g_edTaTitle); g_edTaTitle = NULL; }
    if (g_edTaContent) { RemovePropW(g_edTaContent, PROP_OLDPROC); DestroyWindow(g_edTaContent); g_edTaContent = NULL; }
    if (g_edTaDetail) { RemovePropW(g_edTaDetail, PROP_OLDPROC); DestroyWindow(g_edTaDetail); g_edTaDetail = NULL; }
    if (g_edTaFile) { RemovePropW(g_edTaFile, PROP_OLDPROC); DestroyWindow(g_edTaFile); g_edTaFile = NULL; }

    // BOARD 컨트롤은 board.c가 관리
    Board_DestroyControls();

    // MYTEAM 슬롯은 "파괴"하지 않고, 화면전환 때 숨김만
    ShowMyTeamStatics(0);
}




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

        // ✅ 제외 (너가 말한 것들)
    case SCR_START:
    case SCR_SIGNUP:
    case SCR_FINDPW:
    case SCR_TASK_ADD:
    case SCR_BOARD:
    default:
        return 0;
    }
}

// =========================================================
// [DO NOT TOUCH] 화면별 컨트롤 생성
// =========================================================
static void CreateControlsForScreen(HWND hWnd, Screen s)
{
    DestroyAllEdits();

    // ✅ [추가] 헤더가 필요한 화면이면 팀명/과제명 Edit를 항상 만든다
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

        if (g_edMainTeamName) SetWindowTextW(g_edMainTeamName, g_mainTeamText);
        if (g_edMainTaskName) SetWindowTextW(g_edMainTaskName, g_mainTaskText);
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
        g_edTcTask = CreateEdit(hWnd, 702, 0);
        g_edTcCode = CreateEdit(hWnd, 703, 0);
        break;

    case SCR_TEAM_JOIN:
        g_edTjTeam = CreateEdit(hWnd, 801, 0);
        g_edTjCode = CreateEdit(hWnd, 802, 0);
        break;

    case SCR_TASK_ADD:
        // ✅ (제외 유지) 헤더 안 만들고 과제등록 컨트롤만
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
        // ✅ (제외 유지)
        Board_CreateControls(hWnd);
        break;

    case SCR_MYTEAM:
        // ✅ MYTEAM 슬롯
        EnsureMyTeamStatics(hWnd, GetUIFont());
        MyTeam_RefreshUI(hWnd);
        break;



        // ✅ 이 화면들에도 헤더가 필요하므로 따로 만들 건 없음(헤더는 위에서 생성됨)
    case SCR_DEADLINE:
    case SCR_TODO:
    case SCR_DONE:
        break;

    default:
        break;
    }

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

    if (g_edTaSearch) ShowWindow(g_edTaSearch, SW_HIDE);
    if (g_edTaTask1) ShowWindow(g_edTaTask1, SW_HIDE);
    if (g_edTaTask2) ShowWindow(g_edTaTask2, SW_HIDE);
    if (g_edTaTask3) ShowWindow(g_edTaTask3, SW_HIDE);
    if (g_edTaTask4) ShowWindow(g_edTaTask4, SW_HIDE);
    if (g_edTaTitle) ShowWindow(g_edTaTitle, SW_HIDE);
    if (g_edTaContent) ShowWindow(g_edTaContent, SW_HIDE);
    if (g_edTaDetail) ShowWindow(g_edTaDetail, SW_HIDE);
    if (g_edTaFile) ShowWindow(g_edTaFile, SW_HIDE);

    // ✅ 헤더가 필요한 화면이면 상단 팀명/과제명은 항상 보이게
    if (ScreenHasHeader(g_screen))
    {
        if (g_edMainTeamName) ShowWindow(g_edMainTeamName, SW_SHOW);
        if (g_edMainTaskName) ShowWindow(g_edMainTaskName, SW_SHOW);

        MoveEdit(g_edMainTeamName, SX(R_MAIN_TEAM_X1), SY(R_MAIN_TEAM_Y1),
            SX(R_MAIN_TEAM_X2), SY(R_MAIN_TEAM_Y2), 0, 0, 0, 0);

        MoveEdit(g_edMainTaskName, SX(R_MAIN_TASK_X1), SY(R_MAIN_TASK_Y1),
            SX(R_MAIN_TASK_X2), SY(R_MAIN_TASK_Y2), 0, 0, 0, 0);

        MoveWindow(g_stMainCode, SX(R_MAIN_TASK_X1), SY(R_MAIN_TASK_Y2 + 6),
            SX(R_MAIN_TASK_X2) - SX(R_MAIN_TASK_X1),
            SY(R_MAIN_TASK_Y2 + 28) - SY(R_MAIN_TASK_Y2 + 6),
            TRUE
        );
    }


    // MYTEAM 슬롯도 기본은 숨김 (해당 화면에서만 보이게)
    ShowMyTeamStatics(0);




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


        MoveEdit(g_edMainTeamName, SX(R_MAIN_TEAM_X1), SY(R_MAIN_TEAM_Y1),
            SX(R_MAIN_TEAM_X2), SY(R_MAIN_TEAM_Y2), 0, 0, 0, 0);

        MoveEdit(g_edMainTaskName, SX(R_MAIN_TASK_X1), SY(R_MAIN_TASK_Y1),
            SX(R_MAIN_TASK_X2), SY(R_MAIN_TASK_Y2), 0, 0, 0, 0);


        return;
    }

    // TEAM_CREATE
    if (g_screen == SCR_TEAM_CREATE) {
        ShowWindow(g_edTcTeam, SW_SHOW);
        ShowWindow(g_edTcTask, SW_SHOW);
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

        MoveEdit(g_edTaContent, SX(R_TA_SUBDETAIL_X1), SY(R_TA_SUBDETAIL_Y1),
            SX(R_TA_SUBDETAIL_X2), SY(R_TA_SUBDETAIL_Y2), 0, 0, 0, 0);

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

    // ✅ 과제명은 무조건 공백 고정 (팀 바꿔도 항상 공백)
    if (g_edMainTaskName) SetWindowTextW(g_edMainTaskName, L"");

    // ✅ 코드도 메인에 표시 안 할 거면 비움(원하면 숨김도 가능)
    if (g_stMainCode) {
        SetWindowTextW(g_stMainCode, L"");
        // ShowWindow(g_stMainCode, SW_HIDE); // 아예 숨기고 싶으면 이 줄 켜
    }
}



// task_add helper (forward decl)
static void Task_ClearRightEdits(void);
static void Task_LoadToRightEdits(const TaskItem* t);
static void Task_RefreshLeftList(void);


void App_OnLButtonDown(HWND hWnd, int x, int y)
{
    static int s_in = 0;
    if (s_in) return;
    s_in = 1;

    // ✅ 어떤 경로로든 빠질 때 s_in 풀기용
#define SAFE_LEAVE() do { s_in = 0; return; } while (0)

    g_lastX = x;
    g_lastY = y;
    InvalidateRect(hWnd, NULL, FALSE);

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
            wchar_t task[128] = { 0 };
            wchar_t code[128] = { 0 };

            GetWindowTextW(g_edTcTeam, team, 128);
            GetWindowTextW(g_edTcTask, task, 128);
            GetWindowTextW(g_edTcCode, code, 128);

            if (team[0] == 0 || task[0] == 0 || code[0] == 0) {
                MessageBoxW(hWnd, L"팀명/코드를 모두 입력해 주세요.", L"팀 등록", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (g_currentUserId[0] == 0) {
                MessageBoxW(hWnd, L"로그인 정보가 없습니다. 다시 로그인해 주세요.", L"팀 등록", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            TeamInfo out = { 0 };
            if (!Team_Create(team, task, code, g_currentUserId, &out)) {
                MessageBoxW(hWnd, L"팀 등록 실패!\n(코드 중복이거나 파일 저장 오류일 수 있음)", L"팀 등록", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            lstrcpynW(g_currentTeamId, out.teamId, 64);
            lstrcpynW(g_mainTeamText, out.teamName, 128);

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
            g_mainTaskText[0] = 0;
            g_mainCodeText[0] = 0;

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
            g_mainTaskText[0] = 0;
            g_mainCodeText[0] = 0;

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
    // TASK_ADD
    // -----------------------------------------------------
    if (g_screen == SCR_TASK_ADD)
    {
        if (!g_currentTeamId[0]) {
            MessageBoxW(hWnd, L"팀을 먼저 선택해 주세요(내 팀 저장 후)", L"과제", MB_OK | MB_ICONWARNING);
            SAFE_LEAVE();
        }

        TaskItem* buf = (TaskItem*)calloc(512, sizeof(TaskItem));
        if (!buf) return;
        int n = Task_LoadAll(g_currentTeamId, buf, 512);
        int base = g_taskPage * 4;

        if (HitScaled(R_TA_ITEM1_X1, R_TA_ITEM1_Y1, R_TA_ITEM1_X2, R_TA_ITEM1_Y2, x, y)) {
            int idx = base + 0; if (idx < n) Task_LoadToRightEdits(&buf[idx]);
            SAFE_LEAVE();
        }
        if (HitScaled(R_TA_ITEM2_X1, R_TA_ITEM2_Y1, R_TA_ITEM2_X2, R_TA_ITEM2_Y2, x, y)) {
            int idx = base + 1; if (idx < n) Task_LoadToRightEdits(&buf[idx]);
            SAFE_LEAVE();
        }
        if (HitScaled(R_TA_ITEM3_X1, R_TA_ITEM3_Y1, R_TA_ITEM3_X2, R_TA_ITEM3_Y2, x, y)) {
            int idx = base + 2; if (idx < n) Task_LoadToRightEdits(&buf[idx]);
            SAFE_LEAVE();
        }
        if (HitScaled(R_TA_ITEM4_X1, R_TA_ITEM4_Y1, R_TA_ITEM4_X2, R_TA_ITEM4_Y2, x, y)) {
            int idx = base + 3; if (idx < n) Task_LoadToRightEdits(&buf[idx]);
            SAFE_LEAVE();
        }
        free(buf);

        if (HitScaled(R_TA_BTN_FILE_CLEAR_X1, R_TA_BTN_FILE_CLEAR_Y1, R_TA_BTN_FILE_CLEAR_X2, R_TA_BTN_FILE_CLEAR_Y2, x, y)) {
            if (g_edTaFile) SetWindowTextW(g_edTaFile, L"");
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_BTN_DOWNLOAD_X1, R_TA_BTN_DOWNLOAD_Y1, R_TA_BTN_DOWNLOAD_X2, R_TA_BTN_DOWNLOAD_Y2, x, y)) {
            MessageBoxW(hWnd, L"다운로드 처리 코드를 붙여야 함니다", L"TODO", MB_OK);
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_BTN_FIND_X1, R_TA_BTN_FIND_Y1, R_TA_BTN_FIND_X2, R_TA_BTN_FIND_Y2, x, y))
        {
            wchar_t key[128] = { 0 };
            if (g_edTaSearch) GetWindowTextW(g_edTaSearch, key, 128);
            if (!key[0]) {
                MessageBoxW(hWnd, L"조회 키워드를 입력해 주세요.", L"조회", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            TaskItem found = { 0 };
            if (Task_FindByTitle(g_currentTeamId, key, &found)) {
                Task_LoadToRightEdits(&found);
            }
            else {
                MessageBoxW(hWnd, L"해당 키워드의 과제가 없습니다.", L"조회", MB_OK | MB_ICONINFORMATION);
            }
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_BTN_ADD_X1, R_TA_BTN_ADD_Y1, R_TA_BTN_ADD_X2, R_TA_BTN_ADD_Y2, x, y))
        {
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

            if (!Task_Add(g_currentTeamId, &t)) {
                MessageBoxW(hWnd, L"등록 실패(파일 저장 오류)", L"등록", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_ClearRightEdits();
            Task_RefreshLeftList();
            MessageBoxW(hWnd, L"등록 완료!", L"등록", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_BTN_EDIT_X1, R_TA_BTN_EDIT_Y1, R_TA_BTN_EDIT_X2, R_TA_BTN_EDIT_Y2, x, y))
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

        if (HitScaled(R_TA_BTN_DEL_X1, R_TA_BTN_DEL_Y1, R_TA_BTN_DEL_X2, R_TA_BTN_DEL_Y2, x, y))
        {
            if (g_taskSelectedId == 0) {
                MessageBoxW(hWnd, L"삭제할 과제를 선택해 주세요.", L"삭제", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (!Task_Delete(g_currentTeamId, g_taskSelectedId)) {
                MessageBoxW(hWnd, L"삭제 실패", L"삭제", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_ClearRightEdits();
            Task_RefreshLeftList();
            MessageBoxW(hWnd, L"삭제 완료!", L"삭제", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_BTN_DONE_X1, R_TA_BTN_DONE_Y1, R_TA_BTN_DONE_X2, R_TA_BTN_DONE_Y2, x, y))
        {
            if (g_taskSelectedId == 0) {
                MessageBoxW(hWnd, L"완료할 과제를 선택해 주세요.", L"완료", MB_OK | MB_ICONWARNING);
                SAFE_LEAVE();
            }

            if (!Task_SetDone(g_currentTeamId, g_taskSelectedId, 1)) {
                MessageBoxW(hWnd, L"완료 처리 실패", L"완료", MB_OK | MB_ICONERROR);
                SAFE_LEAVE();
            }

            Task_RefreshLeftList();
            MessageBoxW(hWnd, L"완료 처리!", L"완료", MB_OK | MB_ICONINFORMATION);
            SAFE_LEAVE();
        }

        if (HitScaled(R_TA_PAGE_PREV_X1, R_TA_PAGE_PREV_Y1, R_TA_PAGE_PREV_X2, R_TA_PAGE_PREV_Y2, x, y)) {
            if (g_taskPage > 0) g_taskPage--;
            Task_RefreshLeftList();
            SAFE_LEAVE();
        }
        if (HitScaled(R_TA_PAGE_NEXT_X1, R_TA_PAGE_NEXT_Y1, R_TA_PAGE_NEXT_X2, R_TA_PAGE_NEXT_Y2, x, y)) {
            g_taskPage++;
            Task_RefreshLeftList();
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

    // 기본
    SAFE_LEAVE();
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

    // 화면 BMP
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

    // MYTEAM: 선택된 슬롯 테두리(검정) + 전체 슬롯 테두리(연한)
    if (g_screen == SCR_MYTEAM)
    {
        // 1) 슬롯 각각 연한 테두리
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

        // 2) 선택된 슬롯만 검은 두꺼운 테두리
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


    // 디버그(라스트 클릭)
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
}

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

// =====================================================
// 🔥 TASK_ADD helper 함수들 (여기에 붙여넣기)
// LayoutMyTeamStatics 바로 아래
// =====================================================


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
    // ✅ 스택 대신 힙 사용 (Stack overflow 방지)
    TaskItem* buf = (TaskItem*)calloc(512, sizeof(TaskItem));
    if (!buf) return;

    int n = Task_LoadAll(g_currentTeamId, buf, 512);
    int base = g_taskPage * 4;

    wchar_t t1[128] = L"", t2[128] = L"", t3[128] = L"", t4[128] = L"";

    for (int i = 0; i < 4; i++) {
        int idx = base + i;
        if (idx < n) {
            wchar_t line[140];
            swprintf(line, 140, L"%d. %ls%ls",
                i + 1,
                buf[idx].title,
                buf[idx].done ? L" (완료)" : L"");

            if (i == 0) lstrcpynW(t1, line, 128);
            if (i == 1) lstrcpynW(t2, line, 128);
            if (i == 2) lstrcpynW(t3, line, 128);
            if (i == 3) lstrcpynW(t4, line, 128);
        }
    }

    if (g_edTaTask1) SetWindowTextW(g_edTaTask1, t1);
    if (g_edTaTask2) SetWindowTextW(g_edTaTask2, t2);
    if (g_edTaTask3) SetWindowTextW(g_edTaTask3, t3);
    if (g_edTaTask4) SetWindowTextW(g_edTaTask4, t4);

    free(buf);
}

// =========================================================
// 디버그 오버레이
// =========================================================
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

// DRAWITEM 메시지 처리용 (main.c에서 이걸 호출하는 구조일 때)
LRESULT App_OnDrawItemWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    const DRAWITEMSTRUCT* dis = (const DRAWITEMSTRUCT*)lParam;
    if (dis) {
        // 네가 app.c 안에 만든 함수
        // int App_OnDrawItem(HWND, const DRAWITEMSTRUCT*)
        if (App_OnDrawItem(hWnd, dis)) return TRUE;
    }
    return FALSE;
}

// 마우스 이동 메시지 처리용 (main.c에서 이걸 호출하는 구조일 때)
LRESULT App_OnMouseMoveWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);

    // 필요 없으면 비워둬도 됨(링커용)
    (void)wParam;
    (void)x;
    (void)y;
    (void)hWnd;

    return 0;
}


// =========================================================
// ✅✅✅ (오류 스샷의 LNK2019/LNK2001 "App_OnDrawItem" 해결)
// 기능은 건드리지 않고, 링크만 되게 '빈 구현' 제공
// =========================================================
int App_OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* dis)
{
    (void)hWnd;
    (void)dis;
    return 0;
}

// =========================================================
// ✅✅✅ (오류 스샷의 LNK2019 "App_OnMouseMoveWndProc" 관련 외부기호 해결)
// main.c가 App_OnMouseMove(...)를 찾는 경우가 있어서 동일 심볼 제공
// (기능 변경 없음: 내부에서 WndProc 버전 호출만)
// =========================================================
LRESULT App_OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    return App_OnMouseMoveWndProc(hWnd, wParam, lParam);
}