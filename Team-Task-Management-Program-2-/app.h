// app.h



#pragma once
#include <windows.h>
#include <wchar.h>

// ✅ app.c에 실제로 존재하는 WndProc 헬퍼(네가 올린 app.c 기준)
LRESULT App_OnDrawItemWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT App_OnMouseMoveWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);

// ✅ main.c가 직접 호출하는 함수 시그니처는 "이 파일에서 통일" (링커 깨짐 방지)
// - 네 스샷 오류(LNK2019/LNK2001)는 보통 선언/정의 시그니처 불일치 때문에 발생함.
int App_OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* dis);
// 기존 app.h에 int App_OnMouseMove(HWND,int,int)였는데, app.c 스텁은 LRESULT/ WPARAM/LPARAM로 맞춰둠.
// 👉 기능 안 건드리고 링크만 맞추려면 아래처럼 WndProc 버전만 노출시키는 게 안전함.
LRESULT App_OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam);

void App_GoToStart(HWND hWnd);

typedef enum {
    SCR_START = 0,
    SCR_SIGNUP = 1,
    SCR_MAIN = 2,
    SCR_FINDPW = 3,
    SCR_DEADLINE = 4,     // 마감
    SCR_TODO = 5,         // 미완료
    SCR_MYTEAM = 6,       // 내팀
    SCR_DONE = 7,         // 완료
    SCR_TEAM_CREATE = 8,  // 팀 등록
    SCR_TEAM_JOIN = 9,    // 팀 참여
    SCR_TASK_ADD = 10,
    SCR_BOARD = 11        // 게시판
} Screen;

extern Screen g_screen;

// bmp 파일명
extern const wchar_t* BMP_START;
extern const wchar_t* BMP_SIGNUP;
extern const wchar_t* BMP_MAIN;
extern const wchar_t* BMP_FINDPW;
extern const wchar_t* BMP_DEADLINE;
extern const wchar_t* BMP_TODO;
extern const wchar_t* BMP_MYTEAM;
extern const wchar_t* BMP_DONE;
extern const wchar_t* BMP_TEAM_CREATE;
extern const wchar_t* BMP_TEAM_JOIN;
extern const wchar_t* BMP_TASK_ADD;
extern const wchar_t* BMP_BOARD;

// ✅ app.c에 실제로 있는 전역(네 app.c 기준)
extern wchar_t g_currentUserId[128];
extern wchar_t g_currentTeamId[64];

// ❌ 아래 3개는 app.c에 "정의가 없음" (있으면 ok, 없으면 링크 에러 원인)
//    기능 안 건드리고 문법/링크만 맞추려면 일단 제거하거나,
//    실제로 쓰는 .c에서 정의해줘야 함.
// extern wchar_t g_currentTeamName[128];
// extern wchar_t g_currentTaskName[128];
// extern wchar_t g_currentJoinCode[128];

// bmp 핸들
extern HBITMAP g_bmpStart;
extern HBITMAP g_bmpSignup;
extern HBITMAP g_bmpMain;
extern HBITMAP g_bmpFindPw;
extern HBITMAP g_bmpDeadline;
extern HBITMAP g_bmpTodo;
extern HBITMAP g_bmpMyTeam;
extern HBITMAP g_bmpDone;
extern HBITMAP g_bmpTeamCreate;
extern HBITMAP g_bmpTeamJoin;
extern HBITMAP g_bmpTaskAdd;
extern HBITMAP g_bmpBoard;

extern int g_lastX, g_lastY;
extern int g_clientW, g_clientH;

// 라이프사이클
int  App_OnCreate(HWND hWnd);
void App_OnSize(HWND hWnd, int w, int h);
void App_OnLButtonDown(HWND hWnd, int x, int y);
void App_OnPaint(HWND hWnd, HDC hdc);
void App_OnDestroy(void);

// ✅ 중복 선언 제거 (위에 이미 있음)
// void App_GoToStart(HWND hWnd);

// 전역 Edit HWND 선언
// ❗ 네 app.c는 g_edStartId/g_edStartPw를 static으로 선언해놨음.
//    static이면 extern으로 가져올 수 없음(링커 문제 원인).
//    기능 안 건드린다 했으니, 헤더에서 extern 선언을 제거해서 "링커만" 맞춤.
//// extern HWND g_edStartId;
//// extern HWND g_edStartPw;

// ❌ ListBox 제거했다 했으니 이것도 제거
// extern HWND g_lbMyTeams;

// 내 팀 목록 갱신 + 팀 전환
// (네 app.c에 실제 구현이 없으면 이것들도 링크 에러가 날 수 있음. 사용 안 하면 선언 빼는 게 안전)
void RefreshMyTeamList(HWND hWnd);
void SwitchToTeam(HWND hWnd, const wchar_t* teamId);

HWND App_CreateEdit(HWND parent, int ctrlId, DWORD extraStyle);
#ifndef WM_APP_CHILDCLICK
#define WM_APP_CHILDCLICK (WM_APP + 100)
#endif
