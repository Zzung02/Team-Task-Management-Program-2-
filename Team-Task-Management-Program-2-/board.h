// board.h
#pragma once
#include <windows.h>

void Board_ResetState(void);
void Board_Draw(HDC hdc);                 // ✅ 목록/헤더/선택테두리 그리기
int  Board_OnClick(HWND hWnd, int x, int y); // ✅ 목록 클릭 -> 선택 변경


// app.c 호환용(링크 에러 방지)
void Board_CreateControls(HWND hWnd);
void Board_RelayoutControls(HWND hWnd);
void Board_DestroyControls(void);
void Board_ShowControls(int show);
void Board_ResetState(void);
void Board_Draw(HDC hdc);
int  Board_OnClick(HWND hWnd, int x, int y);

// ✅ 데이터(저장/로드)
int  Board_AddPost(const wchar_t* title, const wchar_t* author, const wchar_t* content);
void Board_Reload(void);