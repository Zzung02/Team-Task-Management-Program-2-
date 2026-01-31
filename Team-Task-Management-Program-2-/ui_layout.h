// ui_layout.h
#pragma once
#include <windows.h>
#include "ui_coords.h"

// app.c에서 관리하는 클라이언트 크기
extern int g_clientW;
extern int g_clientH;


// ---------------------------------------------------------
// ✅ 유틸 함수들
//   - 여기서는 x1,y1,x2,y2는 "이미 스케일된 좌표"만 받는다
//     (즉, 여기서 SX/SY를 다시 적용하면 안 됨)
// ---------------------------------------------------------
int  PtInRectXY(int x, int y, int x1, int y1, int x2, int y2);
void MoveEdit(HWND hEd, int x1, int y1, int x2, int y2, int padL, int padT, int padR, int padB);
