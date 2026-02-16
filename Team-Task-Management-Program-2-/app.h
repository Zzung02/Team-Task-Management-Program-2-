//app.h
#pragma once
#include <windows.h>
#include <wchar.h>

// --------------------------------------------------
// 메시지 정의 (중복 제거)
#ifndef WM_APP_CHILDCLICK
#define WM_APP_CHILDCLICK (WM_APP + 10)
#endif
// --------------------------------------------------

// WndProc 헬퍼
LRESULT App_OnDrawItemWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT App_OnMouseMoveWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT App_OnAppChildClickWndProc(HWND hWnd, WPARAM wParam, LPARAM lParam);

// main.c에서 호출하는 함수
int     App_OnDrawItem(HWND hWnd, const DRAWITEMSTRUCT* dis);
LRESULT App_OnMouseMove(HWND hWnd, WPARAM wParam, LPARAM lParam);

void App_GoToStart(HWND hWnd);

// --------------------------------------------------
// Screen enum (중복/누락 없이 정리)
// --------------------------------------------------
typedef enum {
    SCR_START = 0,
    SCR_SIGNUP,
    SCR_MAIN,
    SCR_FINDPW,

    SCR_DEADLINE,
    SCR_TODO,
    SCR_MYTEAM,
    SCR_DONE,

    SCR_TEAM_CREATE,
    SCR_TEAM_JOIN,

    SCR_TASK_ADD,
    SCR_BOARD,
    SCR_BOARD_WRITE
} Screen;

extern Screen g_screen;

// --------------------------------------------------
// BMP 파일명
// --------------------------------------------------
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

// --------------------------------------------------
// 전역 사용자 정보
// --------------------------------------------------
extern wchar_t g_currentUserId[128];
extern wchar_t g_currentTeamId[64];

// --------------------------------------------------
// BMP 핸들
// --------------------------------------------------
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

// --------------------------------------------------
// 공통 상태
// --------------------------------------------------
extern int g_lastX, g_lastY;
extern int g_clientW, g_clientH;

// --------------------------------------------------
// 라이프사이클
// --------------------------------------------------
int  App_OnCreate(HWND hWnd);
void App_OnSize(HWND hWnd, int w, int h);
void App_OnLButtonDown(HWND hWnd, int x, int y);
void App_OnPaint(HWND hWnd, HDC hdc);
void App_OnDestroy(void);

// --------------------------------------------------
// 팀 관련
// --------------------------------------------------
void RefreshMyTeamList(HWND hWnd);
void SwitchToTeam(HWND hWnd, const wchar_t* teamId);

// Edit 생성 헬퍼
HWND App_CreateEdit(HWND parent, int ctrlId, DWORD extraStyle);
