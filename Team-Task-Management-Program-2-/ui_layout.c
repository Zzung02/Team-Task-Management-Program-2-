// ui_layout.c
#include <windows.h>
#include "ui_layout.h"

// ✅ 여기서는 스케일(SX/SY) 절대 하지 말 것
int PtInRectXY(int x, int y, int x1, int y1, int x2, int y2)
{
    return (x >= x1 && x <= x2 && y >= y1 && y <= y2);
}

// ✅ MoveEdit: 이미 스케일된 좌표(x1,y1,x2,y2)를 그대로 사용
// ui_layout.c
void MoveEdit(HWND hEd, int x1, int y1, int x2, int y2, int padL, int padT, int padR, int padB)
{
    if (!hEd) return;

    int L = x1 + padL;
    int T = y1 + padT;
    int W = (x2 - x1) - (padL + padR);
    int H = (y2 - y1) - (padT + padB);

    if (W < 1) W = 1;
    if (H < 1) H = 1;

    MoveWindow(hEd, L, T, W, H, TRUE);
}
