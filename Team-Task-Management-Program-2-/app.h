#pragma once
#include <windows.h>
#include <wchar.h>

typedef enum {
    SCR_START = 0,
    SCR_SIGNUP = 1,
    SCR_MAIN = 2,
    SCR_FINDPW = 3,
    SCR_DEADLINE = 4,  // 마감
    SCR_TODO = 5,  // 미완료
    SCR_MYTEAM = 6,  // 내팀
    SCR_DONE = 7,  // 완료
    SCR_TEAM_CREATE = 8   // ✅ 팀 등록 화면
} Screen;

extern Screen g_screen;

extern const wchar_t* BMP_START;
extern const wchar_t* BMP_SIGNUP;
extern const wchar_t* BMP_MAIN;
extern const wchar_t* BMP_FINDPW;
extern const wchar_t* BMP_DEADLINE;
extern const wchar_t* BMP_TODO;
extern const wchar_t* BMP_MYTEAM;
extern const wchar_t* BMP_DONE;
extern const wchar_t* BMP_TEAM_CREATE;

extern HBITMAP g_bmpStart;
extern HBITMAP g_bmpSignup;
extern HBITMAP g_bmpMain;
extern HBITMAP g_bmpFindPw;
extern HBITMAP g_bmpDeadline;
extern HBITMAP g_bmpTodo;
extern HBITMAP g_bmpMyTeam;
extern HBITMAP g_bmpDone;
extern HBITMAP g_bmpTeamCreate;

extern int g_lastX, g_lastY;
extern int g_clientW, g_clientH;

int  App_OnCreate(HWND hWnd);
void App_OnSize(HWND hWnd, int w, int h);
void App_OnLButtonDown(HWND hWnd, int x, int y);
void App_OnPaint(HWND hWnd, HDC hdc);
void App_OnDestroy(void);
