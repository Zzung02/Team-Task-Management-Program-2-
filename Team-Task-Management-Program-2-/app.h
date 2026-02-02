#pragma once
#include <windows.h>
#include <wchar.h>


void App_GoToStart(HWND hWnd);

typedef enum {
    SCR_START = 0,
    SCR_SIGNUP = 1,
    SCR_MAIN = 2,
    SCR_FINDPW = 3,
    SCR_DEADLINE = 4,  // 마감
    SCR_TODO = 5,  // 미완료
    SCR_MYTEAM = 6,  // 내팀
    SCR_DONE = 7,  // 완료
    SCR_TEAM_CREATE = 8,// 팀 등록
    SCR_TEAM_JOIN = 9,   // ✅ 팀 참여
    SCR_TASK_ADD = 10,
    SCR_BOARD = 11   // ✅ 게시판

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
extern wchar_t g_currentUserId[128];
extern wchar_t g_currentTeamName[128];
extern wchar_t g_currentTaskName[128];
extern wchar_t g_currentJoinCode[128];

extern HBITMAP g_bmpTaskAdd;
extern const wchar_t* BMP_BOARD;



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
extern HBITMAP g_bmpTeamJoin;            // ✅
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
void App_GoToStart(HWND hWnd);

// 전역 Edit HWND 선언
extern HWND g_edStartId;
extern HWND g_edStartPw;
extern HWND g_lbMyTeams;
// 현재 선택된 팀(캘린더에 보여줄 팀)
extern wchar_t g_currentTeamId[64];
// 내 팀 목록 갱신 + 팀 전환
void RefreshMyTeamList(HWND hWnd);
void SwitchToTeam(HWND hWnd, const wchar_t* teamId);
#pragma once
#include <windows.h>

// ... (기존 선언들)


