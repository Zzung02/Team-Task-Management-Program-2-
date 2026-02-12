// calendar.c  (원샷 덮어쓰기)  ✅ 화면별 클리핑(숫자 튀어나옴 방지) + 주말 빨간색 + 오늘 날짜 굵게
#define _CRT_SECURE_NO_WARNINGS
#include "calendar.h"
#include "ui_coords.h"
#include "task.h"
#include "app.h"   // ✅ g_screen / Screen 사용

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

static wchar_t g_calTeamId[64] = L"";

typedef struct {
    int count;
    wchar_t title[CAL_MAX_EVENTS_PER_DAY][CAL_TITLE_MAX];
} CalDayEvents;

// ------------------------------------------------------------
// 상태
// ------------------------------------------------------------
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

    // 1) YYYY/MM/DD
    if (swscanf(s, L"%d/%d/%d", &yy, &mm, &dd) != 3) {
        // 2) YYYY-MM-DD
        if (swscanf(s, L"%d-%d-%d", &yy, &mm, &dd) != 3) {
            // 3) YYYY.MM.DD
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

// ------------------------------------------------------------
// 외부에서 팀ID 세팅
// ------------------------------------------------------------
void Calendar_SetTeamId(const wchar_t* teamId)
{
    if (!teamId) teamId = L"";
    lstrcpynW(g_calTeamId, teamId, 64);

    if (g_calTeamId[0]) {
        extern void Calendar_RebuildFromTasks(const wchar_t* teamId);
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
// 과제 -> 이벤트(현재 월) 재구성
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

// ------------------------------------------------------------
// 그리기
// ------------------------------------------------------------
void Calendar_Draw(HDC hdc)
{
    int x1 = SX(R_CAL_X1), y1 = SY(R_CAL_Y1);
    int x2 = SX(R_CAL_X2), y2 = SY(R_CAL_Y2);

    int W = x2 - x1;
    int H = y2 - y1;

    const int cols = 7;
    const int rows = 5; // ✅ 네 코드 유지

    int cellW = W / cols;
    int cellH = H / rows;

    // ✅ 오늘 날짜 구하기
    SYSTEMTIME st;
    GetLocalTime(&st);
    int todayY = (int)st.wYear;
    int todayM = (int)st.wMonth;
    int todayD = (int)st.wDay;

    // 0) 월 라벨
    {
        HFONT fMonth = MakeFont(30, 1);
        HFONT old = (HFONT)SelectObject(hdc, fMonth);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));

        wchar_t monthText[64];
        swprintf(monthText, 64, L"%d\n%d월", g_calYear, g_calMonth);

        RECT rc;
        rc.left = SX(R_CAL_LABEL_X1);
        rc.top = SY(R_CAL_LABEL_Y1);
        rc.right = SX(R_CAL_LABEL_X2);
        rc.bottom = SY(R_CAL_LABEL_Y2);

        DrawTextW(hdc, monthText, -1, &rc, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);

        SelectObject(hdc, old);
        DeleteObject(fMonth);
    }

    // ✅✅ 핵심: 화면별로 "실제로 보이는 캘린더 영역"만 그리도록 CLIP
    // (DEADLINE 화면: 수~토만 보이는 레이아웃 / TEAM_CREATE/JOIN: 오른쪽 패널이 덮음)
    int savedDC = SaveDC(hdc);

    int clipL = x1, clipT = y1, clipR = x2, clipB = y2;

    // app.c 전역 g_screen 사용
    extern Screen g_screen;

    if (g_screen == SCR_DEADLINE) {
        // 왼쪽 3칸(일/월/화)은 패널에 가려지므로 그리지 않게
        clipL = x1 + cellW * 3;
    }
    else if (g_screen == SCR_TEAM_CREATE || g_screen == SCR_TEAM_JOIN) {
        // 오른쪽 3칸(목/금/토)은 폼 패널에 가려지므로 그리지 않게
        clipR = x1 + cellW * 4;
    }

    IntersectClipRect(hdc, clipL, clipT, clipR, clipB);

    // 1) 그리드(네 코드 유지: PS_NULL 이면 선이 안 그려짐)
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    for (int c = 0; c <= cols; c++) {
        int x = x1 + (W * c) / cols;
        MoveToEx(hdc, x, y1, NULL);
        LineTo(hdc, x, y2);
    }
    for (int r = 0; r <= rows; r++) {
        int y = y1 + (H * r) / rows;
        MoveToEx(hdc, x1, y, NULL);
        LineTo(hdc, x2, y);
    }

    SelectObject(hdc, oldPen);
    DeleteObject(pen);

    // 2) 날짜/이벤트
    HFONT fDayNormal = MakeFont(16, 0); // 기본
    HFONT fDayBold = MakeFont(16, 1);   // 오늘 날짜용 굵게
    HFONT fEvt = MakeFont(14, 0);

    SetBkMode(hdc, TRANSPARENT);

    int firstDow = DayOfWeek(g_calYear, g_calMonth, 1); // 0=Sun
    int dim = DaysInMonth(g_calYear, g_calMonth);

    for (int day = 1; day <= dim; day++) {
        int idx = firstDow + (day - 1);
        int r = idx / 7;
        int c = idx % 7;

        if (r < 0 || r >= rows) continue;

        int cx = x1 + c * cellW;
        int cy = y1 + r * cellH;

        // ✅ 일/토 빨간색
        int isWeekend = (c == 0 || c == 6);

        // ✅ 오늘 날짜면 굵게
        int isToday = (g_calYear == todayY && g_calMonth == todayM && day == todayD);

        // 날짜 숫자
        {
            wchar_t dtext[8];
            swprintf(dtext, 8, L"%d", day);

            HFONT useFont = isToday ? fDayBold : fDayNormal;
            HFONT old = (HFONT)SelectObject(hdc, useFont);

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

    DeleteObject(fDayNormal);
    DeleteObject(fDayBold);
    DeleteObject(fEvt);

    // ✅ clip 복구
    RestoreDC(hdc, savedDC);
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

    const int cols = 7;
    const int rows = 5;

    int cellW = (W / cols);
    int cellH = (H / rows);

    // ✅✅ 그리기와 동일하게 "화면별 보이는 영역"만 클릭 인정
    extern Screen g_screen;

    int clipL = x1, clipR = x2;
    if (g_screen == SCR_DEADLINE) {
        clipL = x1 + cellW * 3; // 수~토만
    }
    else if (g_screen == SCR_TEAM_CREATE || g_screen == SCR_TEAM_JOIN) {
        clipR = x1 + cellW * 4; // 일~수만
    }
    if (mx < clipL || mx > clipR) return 0;

    int col = (mx - x1) / (cellW ? cellW : 1);
    int row = (my - y1) / (cellH ? cellH : 1);
    if (col < 0) col = 0;
    if (col >= cols) col = cols - 1;
    if (row < 0) row = 0;
    if (row >= rows) row = rows - 1;

    int idx = row * cols + col;

    int firstDow = DayOfWeek(g_calYear, g_calMonth, 1);
    int day = idx - firstDow + 1;

    int dim = DaysInMonth(g_calYear, g_calMonth);
    if (day < 1 || day > dim) return 1;

    return 1;
}
// calendar.c

static int g_calClipMode = 0;

// mode: 0=전체, 1=왼쪽 가림, 2=오른쪽 가림(너가 쓴 기준)
void Calendar_SetClipMode(int mode)
{
    g_calClipMode = mode;
}
