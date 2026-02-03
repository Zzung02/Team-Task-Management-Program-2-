#pragma once
#include <windows.h>

// 게시판 컨트롤 생성/삭제/배치
void Board_CreateControls(HWND hWnd);
void Board_DestroyControls(void);
void Board_RelayoutControls(HWND hWnd);

// 게시판 클릭 처리(버튼들)
int  Board_OnClick(HWND hWnd, int x, int y);

// 파일 저장/로드(간단)
void Board_LoadFromFile(void);
void Board_SaveToFile(void);
