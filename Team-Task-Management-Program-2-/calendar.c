// calendar.c  (원샷 덮어쓰기)
// ✅ 네이버 캘린더 느낌: "월 라벨(2026년 \n 2월)" + ◀▶ 월 이동 + 날짜칸 + (마감일=과제제목 표시)
// ✅ 요일 텍스트 없음
// ✅ App_OnPaint(SCR_MAIN)에서 Calendar_Draw 호출
// ✅ App_OnLButtonDown(SCR_MAIN)에서 Calendar_OnClick 먼저 호출
// ✅ 과제 등록/수정/삭제/완료 성공 시 Calendar_NotifyTasksChanged(hWnd, g_currentTeamId) 호출 권장
// ✅ 화면 전환해도 안 지워지게: teamId 비어있으면 Notify/Rebuild가 Clear만 하지 않도록 방어

#define _CRT_SECURE_NO_WARNINGS
#include "calendar.h"
#include "ui_coords.h"   // SX/SY
#include "task.h"        // Task_LoadAll, TaskItem
#include "app.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CAL_MAX_EVENTS_PER_DAY
#define CAL_MAX_EVENTS_PER_DAY  3
#endif

#ifndef CAL_TITLE_MAX
#define CAL_TITLE_MAX           64
#endif

// ===========================
// ✅ 여기서만 조정하면 됨
// ===========================
#define CAL_COLS 7
#define CAL_ROWS 6   // ✅ 요일줄 없으면 6주(최대 6행) 필요

// 1: 안쪽 선 그리기, 0: 안쪽 선 숨김
#define CAL_DRAW_INNER_LINES 0

// 바깥 테두리(겉선)는 항상 그릴지 (1/0)
#define CAL_DRAW_OUTER_BORDER 1

// 선 두께
#define CAL_BORDER_THICK 2
#define CAL_INNER_THICK  1

// ===========================

static wchar_t g_calTeamId[64] = L"";

typedef struct {
    int count;
    wchar_t title[CAL_MAX_EVENTS_PER_DAY][CAL_TITLE_MAX];
} CalDayEvents;

// 상태
static int g_calYear = 2026;
static int g_calMonth = 1;
static CalDayEvents g_day[32]; // 1..31

// ------------------------------------------------------------
// 유틸
// ------------------------------------------------------------
static int PtInScaled(int x1, int y1, int x2, int y2, int x, int y)
{
    RECT rc;
    rc.left = SX(x1);
    rc.top = SY(y1);
    rc.right = SX(x2);
    rc.bottom = SY(y2);
    if (rc.left > rc.right) { int t = rc.left; rc.left = rc.right; rc.right = t; }
    if (rc.top > rc.bottom) { int t = rc.top; rc.top = rc.bottom; rc.bottom = t; }
    POINT pt = { x, y };
    return PtInRect(&rc, pt);
}

static int ParseDate_YYYYMMDD_local(const wchar_t* s, int* y, int* m, int* d)
{
    if (!s || !s[0]) return 0;

    int yy = 0, mm = 0, dd = 0;

    if (swscanf(s, L"%d/%d/%d", &yy, &mm, &dd) != 3) {
        if (swscanf(s, L"%d-%d-%d", &yy, &mm, &dd) != 3) {
            if (swscanf(s, L"%d.%d.%d", &yy, &mm, &dd) != 3) {
                return 0;
            }
        }
    }

    if (yy < 1900 || mm < 1 || mm > 12 || dd < 1 || dd > 31) return 0;
    *y = yy; *m = mm; *d = dd;
    return 1;
}

static int DaysInMonth(int y, int m)
{
    static const int mdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    int d = mdays[m - 1];
    if (m == 2) {
        int leap = ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
        if (leap) d = 29;
    }
    return d;
}

static int DayOfWeek(int y, int m, int d)
{
    // Zeller’s congruence (0=Sunday)
    if (m < 3) { m += 12; y -= 1; }
    int K = y % 100;
    int J = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7; // 0=Saturday
    int dow = (h + 6) % 7; // 0=Sunday
    return dow;
}

static HFONT MakeFont(int size, int bold)
{
    return CreateFontW(
        size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
}

static void ClearEvents(void)
{
    for (int d = 1; d <= 31; d++) g_day[d].count = 0;
}

// ✅ 선/칸 계산을 “누적 나눗셈”으로 통일 (끝까지 딱 맞음)
static int CellLeft(int x1, int W, int c) { return x1 + (W * c) / CAL_COLS; }
static int CellTop(int y1, int H, int r) { return y1 + (H * r) / CAL_ROWS; }

// ------------------------------------------------------------
// 외부에서 팀ID 세팅
// ------------------------------------------------------------
void Calendar_RebuildFromTasks(const wchar_t* teamId); // forward

void Calendar_SetTeamId(const wchar_t* teamId)
{
    if (!teamId) teamId = L"";
    lstrcpynW(g_calTeamId, teamId, 64);

    if (g_calTeamId[0]) {
        Calendar_RebuildFromTasks(g_calTeamId);
    }
}

// ------------------------------------------------------------
// 초기화
// ------------------------------------------------------------
void Calendar_Init(void)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    g_calYear = st.wYear;
    g_calMonth = st.wMonth;
    ClearEvents();
}

void Calendar_SetYM(int year, int month)
{
    if (year < 1900) year = 1900;
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    g_calYear = year;
    g_calMonth = month;
}

void Calendar_GetYM(int* outYear, int* outMonth)
{
    if (outYear) *outYear = g_calYear;
    if (outMonth) *outMonth = g_calMonth;
}

// ------------------------------------------------------------
// 과제 -> 이벤트 재구성
// ------------------------------------------------------------
void Calendar_RebuildFromTasks(const wchar_t* teamId)
{
    if (!teamId || !teamId[0]) return;

    ClearEvents();

    TaskItem* all = (TaskItem*)calloc(512, sizeof(TaskItem));
    if (!all) return;

    int nAll = Task_LoadAll(teamId, all, 512);

    for (int i = 0; i < nAll; i++) {
        if (all[i].title[0] == 0) continue;
        if (all[i].done == 1) continue;
        if (all[i].deadline[0] == 0) continue;

        int y = 0, m = 0, d = 0;
        if (!ParseDate_YYYYMMDD_local(all[i].deadline, &y, &m, &d)) continue;
        if (y != g_calYear || m != g_calMonth) continue;
        if (d < 1 || d > 31) continue;

        CalDayEvents* day = &g_day[d];
        if (day->count < CAL_MAX_EVENTS_PER_DAY) {
            lstrcpynW(day->title[day->count], all[i].title, CAL_TITLE_MAX);
            day->count++;
        }
    }

    free(all);
}

void Calendar_NotifyTasksChanged(HWND hWnd, const wchar_t* teamId)
{
    if (!teamId || !teamId[0]) return;
    Calendar_RebuildFromTasks(teamId);
    if (hWnd) InvalidateRect(hWnd, NULL, FALSE);
}

// ===============================
// ✅ 캘린더 "칸 크기 직접 조정" 버전
// - cellW/cellH 숫자만 바꾸면 칸 넓이/높이 변경됨
// - 그리드 위치는 R_CAL_X1/Y1 기준 + 오프셋으로 미세조정 가능
// ===============================

void Calendar_Draw(HDC hdc)
{

    // ------------------------------
    // 1) 그리드 실제 위치 계산
    // ------------------------------
    int baseX1 = SX(R_CAL_X1);
    int baseY1 = SY(R_CAL_Y1);

    int x1 = baseX1 + CAL_GRID_OFFX;
    int y1 = baseY1 + CAL_GRID_OFFY;

    int W = CAL_CELL_W * CAL_COLS;
    int H = CAL_CELL_H * CAL_ROWS;

    int x2 = x1 + W;
    int y2 = y1 + H;

    // ------------------------------
    // 2) 월 라벨(2026년 \n 2월) - 너 좌표 그대로 사용
    // ------------------------------
    {
        HFONT fMonth = MakeFont(30, 1);
        HFONT old = (HFONT)SelectObject(hdc, fMonth);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));

        wchar_t monthText[64];
        swprintf(monthText, 64, L"%d년\n%d월", g_calYear, g_calMonth);

        RECT rc;
        rc.left = SX(R_CAL_LABEL_X1);
        rc.top = SY(R_CAL_LABEL_Y1);
        rc.right = SX(R_CAL_LABEL_X2);
        rc.bottom = SY(R_CAL_LABEL_Y2);

        DrawTextW(hdc, monthText, -1, &rc,
            DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);

        SelectObject(hdc, old);
        DeleteObject(fMonth);
    }

    // ------------------------------
    // 3) 선(지금은 "선 없애고 싶다" 했으니까 OFF)
    //    - 다시 켜고 싶으면 if(1)로 바꾸기
    // ------------------------------
    if (0)
    {
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
        HGDIOBJ oldPen = SelectObject(hdc, pen);

        for (int c = 0; c <= CAL_COLS; c++) {
            int x = x1 + c * CAL_CELL_W;
            MoveToEx(hdc, x, y1, NULL);
            LineTo(hdc, x, y2);
        }
        for (int r = 0; r <= CAL_ROWS; r++) {
            int y = y1 + r * CAL_CELL_H;
            MoveToEx(hdc, x1, y, NULL);
            LineTo(hdc, x2, y);
        }

        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    // ------------------------------
    // 4) 날짜 / 이벤트
    //    - 토/일 숫자 빨간색 처리
    // ------------------------------
    HFONT fDay = MakeFont(16, 1);
    HFONT fEvt = MakeFont(14, 0);

    SetBkMode(hdc, TRANSPARENT);

    int firstDow = DayOfWeek(g_calYear, g_calMonth, 1); // 0=Sun
    int dim = DaysInMonth(g_calYear, g_calMonth);

    for (int day = 1; day <= dim; day++) {
        int idx = firstDow + (day - 1);
        int r = idx / CAL_COLS;
        int c = idx % CAL_COLS;
        if (r < 0 || r >= CAL_ROWS) continue;

        int cx = x1 + c * CAL_CELL_W;
        int cy = y1 + r * CAL_CELL_H;

        // ✅ 일/토 텍스트 빨간색
        // col 0 = 일요일, col 6 = 토요일 (firstDow 반영된 idx 기준)
        int isWeekend = (c == 0 || c == 6);

        // 날짜 숫자
        {
            wchar_t dtext[8];
            swprintf(dtext, 8, L"%d", day);

            HFONT old = (HFONT)SelectObject(hdc, fDay);
            SetTextColor(hdc, isWeekend ? RGB(220, 0, 0) : RGB(0, 0, 0));
            TextOutW(hdc, cx + 6, cy + 4, dtext, (int)wcslen(dtext));
            SelectObject(hdc, old);
        }

        // 이벤트(과제 제목)
        if (g_day[day].count > 0) {
            HFONT old2 = (HFONT)SelectObject(hdc, fEvt);
            SetTextColor(hdc, RGB(0, 0, 0));

            int yy = cy + 24;
            for (int k = 0; k < g_day[day].count; k++) {
                wchar_t line[CAL_TITLE_MAX + 8];
                lstrcpynW(line, g_day[day].title[k], CAL_TITLE_MAX);
                if ((int)wcslen(line) > 12) { line[12] = 0; wcscat(line, L".."); }

                TextOutW(hdc, cx + 6, yy, line, (int)wcslen(line));
                yy += 18;
            }
            SelectObject(hdc, old2);
        }
    }

    DeleteObject(fDay);
    DeleteObject(fEvt);
}


// ------------------------------------------------------------
// 클릭 처리(◀▶ 월 이동, 날짜칸 클릭)
// ------------------------------------------------------------
int Calendar_OnClick(HWND hWnd, int mx, int my)
{
    // ◀
    if (PtInScaled(R_CAL_PREV_X1, R_CAL_PREV_Y1, R_CAL_PREV_X2, R_CAL_PREV_Y2, mx, my)) {
        g_calMonth--;
        if (g_calMonth < 1) { g_calMonth = 12; g_calYear--; }
        Calendar_RebuildFromTasks(g_calTeamId);
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    // ▶
    if (PtInScaled(R_CAL_NEXT_X1, R_CAL_NEXT_Y1, R_CAL_NEXT_X2, R_CAL_NEXT_Y2, mx, my)) {
        g_calMonth++;
        if (g_calMonth > 12) { g_calMonth = 1; g_calYear++; }
        Calendar_RebuildFromTasks(g_calTeamId);
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    // 캘린더 영역 체크
    int x1 = SX(R_CAL_X1), y1 = SY(R_CAL_Y1);
    int x2 = SX(R_CAL_X2), y2 = SY(R_CAL_Y2);
    if (mx < x1 || mx > x2 || my < y1 || my > y2) return 0;

    int W = x2 - x1;
    int H = y2 - y1;

    // ✅ 클릭도 “누적 나눗셈”과 동일한 칸 기준으로 계산
    int col = (int)(((long long)(mx - x1) * CAL_COLS) / (W ? W : 1));
    int row = (int)(((long long)(my - y1) * CAL_ROWS) / (H ? H : 1));

    if (col < 0) col = 0;
    if (col >= CAL_COLS) col = CAL_COLS - 1;
    if (row < 0) row = 0;
    if (row >= CAL_ROWS) row = CAL_ROWS - 1;

    int idx = row * CAL_COLS + col;

    int firstDow = DayOfWeek(g_calYear, g_calMonth, 1);
    int day = idx - firstDow + 1;

    int dim = DaysInMonth(g_calYear, g_calMonth);
    if (day < 1 || day > dim) return 1;

    // (원하면 여기서 day 클릭 기능)
    // wchar_t msg[64];
    // swprintf(msg, 64, L"%d년 %d월 %d일 클릭!", g_calYear, g_calMonth, day);
    // MessageBoxW(hWnd, msg, L"Calendar", MB_OK);

    return 1;
}
