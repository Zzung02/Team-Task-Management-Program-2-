// board.c  (원샷 덮어쓰기)  ✅ Edit 없이 직접 그리기/클릭 선택
#define _CRT_SECURE_NO_WARNINGS
#include "board.h"
#include "ui_coords.h"   // SX/SY + R_BOARD_LIST_...
#include <windows.h>

#ifndef BD_ROWS_PER_PAGE
#define BD_ROWS_PER_PAGE 10
#endif

// ✅ 선택된 "보이는 줄" (0~9)
static int g_selRow = -1;
static int g_page = 0;

void Board_ResetState(void)
{
    g_selRow = -1;
    g_page = 0;
}

static RECT RcScaled(int x1, int y1, int x2, int y2)
{
    RECT r;
    r.left = SX(x1); r.top = SY(y1);
    r.right = SX(x2); r.bottom = SY(y2);
    if (r.left > r.right) { int t = r.left; r.left = r.right; r.right = t; }
    if (r.top > r.bottom) { int t = r.top;  r.top = r.bottom; r.bottom = t; }
    return r;
}
static int PtInRc(RECT r, int x, int y)
{
    return (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom);
}

// ✅ 목록 영역에서 클릭한 y로 몇번째 줄인지 계산
static int HitRowIndex(int x, int y, RECT rcList)
{
    if (!PtInRc(rcList, x, y)) return -1;

    int listH = rcList.bottom - rcList.top;
    if (listH <= 0) return -1;

    int rowH = listH / BD_ROWS_PER_PAGE;         // 10줄로 쪼갬
    if (rowH <= 0) rowH = 1;

    int relY = y - rcList.top;
    int row = relY / rowH;
    if (row < 0 || row >= BD_ROWS_PER_PAGE) return -1;
    return row;
}

void Board_Draw(HDC hdc)
{
    // ✅ 너 ui_coords.h에 이미 있는 목록 큰 사각형
    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);

    int w = rcList.right - rcList.left;
    int h = rcList.bottom - rcList.top;
    if (w <= 0 || h <= 0) return;

    // ----------------------------
    // 1) 컬럼(번호/제목/글쓴이) 구분
    //    너 BMP 라벨 위치랑 맞게 비율로 칼럼 나눔
    // ----------------------------
    int colNumW = (int)(w * 0.05);  // 번호 칼럼 폭
    int colAuthorW = (int)(w * 0.15);  // 글쓴이 칼럼 폭
    int colTitleW = w - colNumW - colAuthorW;

    int xNumL = rcList.left;
    int xNumR = rcList.left + colNumW;

    int xTitleL = xNumR;
    int xTitleR = xTitleL + colTitleW;

    int xAuthorL = xTitleR;
    int xAuthorR = rcList.right;

    // ----------------------------
    // 2) 줄(10개) 계산
    // ----------------------------
    int rowH = h / BD_ROWS_PER_PAGE;
    if (rowH <= 0) rowH = 1;

    HPEN penThin = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, penThin);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    // ✅ 전체 테두리
    Rectangle(hdc, rcList.left, rcList.top, rcList.right, rcList.bottom);

    // ✅ 행/열 칸 테두리 그리기 + 텍스트(샘플)
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < BD_ROWS_PER_PAGE; i++)
    {
        int y1 = rcList.top + i * rowH;
        int y2 = (i == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        // 칸: 번호 / 제목 / 글쓴이
        Rectangle(hdc, xNumL, y1, xNumR, y2);
        Rectangle(hdc, xTitleL, y1, xTitleR, y2);
        Rectangle(hdc, xAuthorL, y1, xAuthorR, y2);

        // ---- (지금은 샘플 텍스트) ----
        // 실제 데이터 붙일 때 여기만 바꾸면 됨
        wchar_t buf[256];

        // 번호
        wsprintfW(buf, L"%d", (g_page * BD_ROWS_PER_PAGE) + (i + 1));
        TextOutW(hdc, xNumL + 8, y1 + 8, buf, lstrlenW(buf));

        // 제목(샘플)
        lstrcpyW(buf, L"");
        // TextOutW(hdc, xTitleL + 8, y1 + 8, buf, lstrlenW(buf));

        // 글쓴이(샘플)
        lstrcpyW(buf, L"");
        // TextOutW(hdc, xAuthorL + 8, y1 + 8, buf, lstrlenW(buf));
    }

    // ----------------------------
    // 3) 선택 테두리(굵게) 표시
    // ----------------------------
    if (g_selRow >= 0 && g_selRow < BD_ROWS_PER_PAGE)
    {
        int y1 = rcList.top + g_selRow * rowH;
        int y2 = (g_selRow == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        HPEN penBold = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
        SelectObject(hdc, penBold);

        // ✅ 선택 줄 전체(3칸 합친 영역) 굵은 테두리
        Rectangle(hdc, rcList.left + 1, y1 + 1, rcList.right - 1, y2 - 1);

        SelectObject(hdc, penThin);
        DeleteObject(penBold);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(penThin);
}

int Board_OnClick(HWND hWnd, int x, int y)
{
    // ✅ 페이지 버튼은 너가 이미 HitScaledRect 좌표를 쓰고 있으면 여기에도 붙일 수 있음
    // 지금은 목록 클릭만 처리

    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);
    int row = HitRowIndex(x, y, rcList);

    if (row >= 0)
    {
        g_selRow = row;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }
    return 0;
}


// ---- app.c가 아직 호출하는 함수들(이제는 안 쓰지만 링크 에러 방지용) ----
void Board_CreateControls(HWND hWnd) { (void)hWnd; }
void Board_RelayoutControls(HWND hWnd) { (void)hWnd; }
void Board_DestroyControls(void) {}
void Board_ShowControls(int show) { (void)show; }
