// board.c  ✅ 직접 그리기 + 팀별 파일 + 페이지(◀▶) + 수정/삭제 + 제목 조회(완전일치) + 페이지번호 표시
// ✅ "팀 선택처럼" : 데이터 있는 줄만 선택(굵은 테두리), 빈 줄 클릭 시 선택 안 됨
#define _CRT_SECURE_NO_WARNINGS
#include "board.h"
#include "ui_coords.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#ifndef BD_ROWS_PER_PAGE
#define BD_ROWS_PER_PAGE 10
#endif

#define BD_MAX_POSTS   512
#define BD_TITLE_MAX   128
#define BD_AUTHOR_MAX  128
#define BD_CONTENT_MAX 2048

typedef struct {
    int id;
    wchar_t title[BD_TITLE_MAX];
    wchar_t author[BD_AUTHOR_MAX];
    wchar_t content[BD_CONTENT_MAX];
} BoardPost;

static BoardPost g_posts[BD_MAX_POSTS];
static int g_postCount = 0;

static int g_selRow = -1;   // 0..9 (보이는 줄)
static int g_page = 0;      // 0..

static wchar_t g_boardTeamId[64] = L"";

// ✅ 조회(제목) 필터 뷰 (지금은 전체 리스트 유지 + 점프만)
static int g_viewIdx[BD_MAX_POSTS];
static int g_viewCount = 0;
static wchar_t g_searchQuery[BD_TITLE_MAX] = L"";

// ✅ invalidate 대상 윈도우 저장 (GetActiveWindow() 쓰면 갱신 삑남)
static HWND g_hostWnd = NULL;

// --------------------
// util
// --------------------
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

static void TrimNL(wchar_t* s)
{
    if (!s) return;
    size_t n = wcslen(s);
    while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) {
        s[n - 1] = 0;
        n--;
    }
}

static int HitRowIndex(int x, int y, RECT rcList)
{
    if (!PtInRc(rcList, x, y)) return -1;

    int listH = rcList.bottom - rcList.top;
    if (listH <= 0) return -1;

    int rowH = listH / BD_ROWS_PER_PAGE;
    if (rowH <= 0) rowH = 1;

    int relY = y - rcList.top;
    int row = relY / rowH;

    if (row < 0 || row >= BD_ROWS_PER_PAGE) return -1;
    return row;
}

static void BuildBoardPath(const wchar_t* teamId, wchar_t* outPath, int cap)
{
    if (!outPath || cap <= 0) return;

    if (!teamId || !teamId[0]) {
        wcsncpy(outPath, L"board_common.txt", cap - 1);
        outPath[cap - 1] = 0;
        return;
    }
    _snwprintf_s(outPath, cap, _TRUNCATE, L"board_%s.txt", teamId);
    outPath[cap - 1] = 0;
}

// - \n  ->  \\n
// - \   ->  \\
// - |   ->  \\p
static void EscapeField(const wchar_t* in, wchar_t* out, int cap)
{
    if (!out || cap <= 0) return;
    int oi = 0;
    for (int i = 0; in && in[i] && oi < cap - 1; i++) {
        wchar_t ch = in[i];
        if (ch == L'\r') continue;

        if (ch == L'\n') {
            if (oi < cap - 2) { out[oi++] = L'\\'; out[oi++] = L'n'; }
        }
        else if (ch == L'\\') {
            if (oi < cap - 2) { out[oi++] = L'\\'; out[oi++] = L'\\'; }
        }
        else if (ch == L'|') {
            if (oi < cap - 2) { out[oi++] = L'\\'; out[oi++] = L'p'; }
        }
        else {
            out[oi++] = ch;
        }
    }
    out[oi] = 0;
}

static void UnescapeField(const wchar_t* in, wchar_t* out, int cap)
{
    if (!out || cap <= 0) return;
    int oi = 0;
    for (int i = 0; in && in[i] && oi < cap - 1; i++) {
        if (in[i] == L'\\') {
            wchar_t n = in[i + 1];
            if (n == L'n') { out[oi++] = L'\n'; i++; continue; }
            if (n == L'\\') { out[oi++] = L'\\'; i++; continue; }
            if (n == L'p') { out[oi++] = L'|';  i++; continue; }
        }
        out[oi++] = in[i];
    }
    out[oi] = 0;
}

static int Board_NextId_FromCache(void)
{
    int maxId = 0;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id > maxId) maxId = g_posts[i].id;
    }
    return maxId + 1;
}

// ✅ 뷰 재구성 (현재는 전체 유지)
static void Board_RebuildView(void)
{
    g_viewCount = 0;
    for (int i = 0; i < g_postCount && g_viewCount < BD_MAX_POSTS; i++) {
        g_viewIdx[g_viewCount++] = i;
    }
}

static int Board_SaveAll(void)
{
    wchar_t path[260];
    BuildBoardPath(g_boardTeamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"w, ccs=UTF-8");
    if (!fp) return 0;

    for (int i = 0; i < g_postCount; i++) {
        wchar_t t2[BD_TITLE_MAX * 2] = { 0 };
        wchar_t a2[BD_AUTHOR_MAX * 2] = { 0 };
        wchar_t c2[BD_CONTENT_MAX * 2] = { 0 };

        EscapeField(g_posts[i].title, t2, (int)(sizeof(t2) / sizeof(t2[0])));
        EscapeField(g_posts[i].author, a2, (int)(sizeof(a2) / sizeof(a2[0])));
        EscapeField(g_posts[i].content, c2, (int)(sizeof(c2) / sizeof(c2[0])));

        fwprintf(fp, L"%d|%s|%s|%s\n", g_posts[i].id, t2, a2, c2);
    }

    fclose(fp);
    return 1;
}

// --------------------
// public API
// --------------------
void Board_SetTeamId(const wchar_t* teamId)
{
    if (!teamId) teamId = L"";
    lstrcpynW(g_boardTeamId, teamId, 64);
    Board_ResetState();
    Board_Reload();
}

void Board_ResetState(void)
{
    g_selRow = -1;
    g_page = 0;
}

void Board_Reload(void)
{
    g_postCount = 0;

    wchar_t path[260];
    BuildBoardPath(g_boardTeamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"r, ccs=UTF-8");
    if (!fp) {
        Board_RebuildView();
        return;
    }

    wchar_t line[8192];

    while (fgetws(line, (int)(sizeof(line) / sizeof(line[0])), fp) && g_postCount < BD_MAX_POSTS) {
        TrimNL(line);
        if (line[0] == 0) continue;

        wchar_t* p1 = wcschr(line, L'|');
        if (!p1) continue;
        *p1++ = 0;

        wchar_t* p2 = wcschr(p1, L'|');
        if (!p2) continue;
        *p2++ = 0;

        wchar_t* p3 = wcschr(p2, L'|');
        if (!p3) continue;
        *p3++ = 0;

        int id = _wtoi(line);
        if (id <= 0) continue;

        BoardPost p;
        ZeroMemory(&p, sizeof(p));
        p.id = id;

        UnescapeField(p1, p.title, BD_TITLE_MAX);
        UnescapeField(p2, p.author, BD_AUTHOR_MAX);
        UnescapeField(p3, p.content, BD_CONTENT_MAX);

        g_posts[g_postCount++] = p;
    }

    fclose(fp);

    Board_RebuildView();

    int maxPage = (g_viewCount <= 0) ? 0 : ((g_viewCount - 1) / BD_ROWS_PER_PAGE);
    if (g_page > maxPage) g_page = maxPage;
    if (g_selRow >= BD_ROWS_PER_PAGE) g_selRow = -1;
}

int Board_AddPost(const wchar_t* title, const wchar_t* author, const wchar_t* content)
{
    if (!title || !title[0] || !author || !author[0]) return 0;

    Board_Reload(); // id 중복 방지

    wchar_t path[260];
    BuildBoardPath(g_boardTeamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"a, ccs=UTF-8");
    if (!fp) return 0;

    int id = Board_NextId_FromCache();

    wchar_t t2[BD_TITLE_MAX * 2] = { 0 };
    wchar_t a2[BD_AUTHOR_MAX * 2] = { 0 };
    wchar_t c2[BD_CONTENT_MAX * 2] = { 0 };

    EscapeField(title, t2, (int)(sizeof(t2) / sizeof(t2[0])));
    EscapeField(author, a2, (int)(sizeof(a2) / sizeof(a2[0])));
    EscapeField(content ? content : L"", c2, (int)(sizeof(c2) / sizeof(c2[0])));

    fwprintf(fp, L"%d|%s|%s|%s\n", id, t2, a2, c2);
    fclose(fp);

    Board_Reload();
    return 1;
}

int Board_UpdatePost(int id, const wchar_t* newTitle, const wchar_t* newContent)
{
    if (id <= 0) return 0;
    if (!newTitle || !newTitle[0]) return 0;

    Board_Reload();

    int found = -1;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id == id) { found = i; break; }
    }
    if (found < 0) return 0;

    lstrcpynW(g_posts[found].title, newTitle, BD_TITLE_MAX);
    lstrcpynW(g_posts[found].content, newContent ? newContent : L"", BD_CONTENT_MAX);

    if (!Board_SaveAll()) return 0;
    Board_Reload();
    return 1;
}

int Board_DeletePost(int id)
{
    if (id <= 0) return 0;

    Board_Reload();

    int found = -1;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id == id) { found = i; break; }
    }
    if (found < 0) return 0;

    for (int i = found; i < g_postCount - 1; i++) {
        g_posts[i] = g_posts[i + 1];
    }
    g_postCount--;

    if (!Board_SaveAll()) return 0;

    Board_RebuildView();
    int maxPage = (g_viewCount <= 0) ? 0 : ((g_viewCount - 1) / BD_ROWS_PER_PAGE);
    if (g_page > maxPage) g_page = maxPage;
    g_selRow = -1;

    Board_Reload();
    return 1;
}

// =============================
// 조회 API
// =============================
void Board_SetSearchQuery(const wchar_t* q)
{
    if (!q) q = L"";
    lstrcpynW(g_searchQuery, q, BD_TITLE_MAX);
}

static void Board_RequestRedraw(void)
{
    if (g_hostWnd && IsWindow(g_hostWnd)) {
        InvalidateRect(g_hostWnd, NULL, FALSE);
    }
    else {
        HWND w = GetActiveWindow();
        if (w) InvalidateRect(w, NULL, FALSE);
    }
}

void Board_ApplySearch(void)
{
    if (!g_searchQuery[0]) {
        g_selRow = -1;
        Board_RequestRedraw();
        return;
    }

    int foundPos = -1;
    for (int pos = 0; pos < g_viewCount; pos++) {
        int realIdx = g_viewIdx[pos];
        if (realIdx < 0 || realIdx >= g_postCount) continue;

        if (lstrcmpW(g_posts[realIdx].title, g_searchQuery) == 0) {
            foundPos = pos;
            break;
        }
    }

    if (foundPos < 0) {
        g_selRow = -1;
        Board_RequestRedraw();
        return;
    }

    g_page = foundPos / BD_ROWS_PER_PAGE;
    g_selRow = foundPos % BD_ROWS_PER_PAGE;

    Board_RequestRedraw();
}

void Board_ClearSearch(void)
{
    g_searchQuery[0] = 0;
    g_selRow = -1;
    Board_RequestRedraw();
}

// =============================
// 선택/조회
// =============================
int Board_GetSelectedIndex(void) { return g_selRow; }

int Board_GetSelectedPostId(void)
{
    if (g_selRow < 0 || g_selRow >= BD_ROWS_PER_PAGE) return 0;
    int viewPos = g_page * BD_ROWS_PER_PAGE + g_selRow;
    if (viewPos < 0 || viewPos >= g_viewCount) return 0;

    int realIdx = g_viewIdx[viewPos];
    if (realIdx < 0 || realIdx >= g_postCount) return 0;

    return g_posts[realIdx].id;
}

int Board_GetPostById(int id,
    wchar_t* outTitle, int tCap,
    wchar_t* outAuthor, int aCap,
    wchar_t* outContent, int cCap)
{
    if (id <= 0) return 0;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id == id) {
            if (outTitle && tCap > 0)   lstrcpynW(outTitle, g_posts[i].title, tCap);
            if (outAuthor && aCap > 0) lstrcpynW(outAuthor, g_posts[i].author, aCap);
            if (outContent && cCap > 0) lstrcpynW(outContent, g_posts[i].content, cCap);
            return 1;
        }
    }
    return 0;
}

// --------------------
// draw helpers
// --------------------
static void DrawCellText(HDC hdc, RECT rc, const wchar_t* text)
{
    RECT r = rc;
    r.left += 8;
    r.right -= 8;

    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text ? text : L"", -1, &r,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void DrawPageLabelCenter(HDC hdc)
{
    RECT rc = RcScaled(R_BD_PAGE_LABEL_X1, R_BD_PAGE_LABEL_Y1,
                       R_BD_PAGE_LABEL_X2, R_BD_PAGE_LABEL_Y2);

    int maxPage = (g_viewCount <= 0) ? 0 : ((g_viewCount - 1) / BD_ROWS_PER_PAGE);

    wchar_t buf[64];
    wsprintfW(buf, L"%d", g_page + 1);

    HFONT f = CreateFontW(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HFONT oldF = (HFONT)SelectObject(hdc, f);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    DrawTextW(hdc, buf, -1, &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc, oldF);
    DeleteObject(f);
}
void Board_Draw(HDC hdc)
{
    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);

    int w = rcList.right - rcList.left;
    int h = rcList.bottom - rcList.top;
    if (w <= 0 || h <= 0) return;

    int colNumW = (int)(w * 0.12);
    int colAuthorW = (int)(w * 0.15);
    int colTitleW = w - colNumW - colAuthorW;

    int xNumL = rcList.left;
    int xNumR = rcList.left + colNumW;

    int xTitleL = xNumR;
    int xTitleR = xTitleL + colTitleW;

    int xAuthorL = xTitleR;
    int xAuthorR = rcList.right;

    int rowH = h / BD_ROWS_PER_PAGE;
    if (rowH <= 0) rowH = 1;

    HPEN penThin = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, penThin);
    HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

    // ✅ 1) 바깥 테두리 1번
    Rectangle(hdc, rcList.left, rcList.top, rcList.right, rcList.bottom);

    // ✅ 2) 세로선(컬럼 경계) 1번씩
    MoveToEx(hdc, xNumR, rcList.top, NULL);    LineTo(hdc, xNumR, rcList.bottom);
    MoveToEx(hdc, xTitleR, rcList.top, NULL);  LineTo(hdc, xTitleR, rcList.bottom);

    // ✅ 3) 가로선(행 경계) 1번씩 (맨 아래는 외곽선이니까 제외)
    for (int i = 1; i < BD_ROWS_PER_PAGE; i++) {
        int y = rcList.top + i * rowH;
        MoveToEx(hdc, rcList.left, y, NULL);
        LineTo(hdc, rcList.right, y);
    }

    // ✅ 텍스트 출력
    int start = g_page * BD_ROWS_PER_PAGE;

    for (int i = 0; i < BD_ROWS_PER_PAGE; i++) {
        int y1 = rcList.top + i * rowH;
        int y2 = (i == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        RECT rcNum = { xNumL, y1, xNumR, y2 };
        RECT rcTitle = { xTitleL, y1, xTitleR, y2 };
        RECT rcAuthor = { xAuthorL, y1, xAuthorR, y2 };

        int viewPos = start + i;

        // 번호(항상 1.. 표시)
        wchar_t numBuf[32] = L"";
        wsprintfW(numBuf, L"%d", viewPos + 1);
        DrawCellText(hdc, rcNum, numBuf);

        // 데이터
        if (viewPos < g_viewCount) {
            int realIdx = g_viewIdx[viewPos];
            if (realIdx >= 0 && realIdx < g_postCount) {
                DrawCellText(hdc, rcTitle, g_posts[realIdx].title);
                DrawCellText(hdc, rcAuthor, g_posts[realIdx].author);
            }
        }
    }

    // ✅ 선택 테두리(굵게) - ✅ 데이터 있는 줄에만!
    if (g_selRow >= 0 && g_selRow < BD_ROWS_PER_PAGE) {
        int viewPos = g_page * BD_ROWS_PER_PAGE + g_selRow;
        if (viewPos >= 0 && viewPos < g_viewCount) {
            int y1 = rcList.top + g_selRow * rowH;
            int y2 = (g_selRow == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

            HPEN penBold = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
            HGDIOBJ oldPen2 = SelectObject(hdc, penBold);
            HGDIOBJ oldBrush2 = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));

            Rectangle(hdc, rcList.left + 1, y1 + 1, rcList.right - 1, y2 - 1);

            SelectObject(hdc, oldBrush2);
            SelectObject(hdc, oldPen2);
            DeleteObject(penBold);
        }
    }

    DrawPageLabelCenter(hdc);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(penThin);
}

int Board_OnClick(HWND hWnd, int x, int y)
{
    // hostWnd 업데이트(혹시 CreateControls 안 불리는 구조 대비)
    if (!g_hostWnd) g_hostWnd = hWnd;

    if (PtInRc(RcScaled(R_BD_PAGE_PREV_X1, R_BD_PAGE_PREV_Y1, R_BD_PAGE_PREV_X2, R_BD_PAGE_PREV_Y2), x, y)) {
        if (g_page > 0) g_page--;
        g_selRow = -1;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    if (PtInRc(RcScaled(R_BD_PAGE_NEXT_X1, R_BD_PAGE_NEXT_Y1, R_BD_PAGE_NEXT_X2, R_BD_PAGE_NEXT_Y2), x, y)) {
        int maxPage = (g_viewCount <= 0) ? 0 : ((g_viewCount - 1) / BD_ROWS_PER_PAGE);
        if (g_page < maxPage) g_page++;
        g_selRow = -1;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);
    int row = HitRowIndex(x, y, rcList);
    if (row >= 0) {
        int viewPos = g_page * BD_ROWS_PER_PAGE + row;

        // ✅ 데이터 없는 줄 클릭이면 선택 안 함(팀선택처럼)
        if (viewPos < 0 || viewPos >= g_viewCount) {
            g_selRow = -1; // 선택 해제 (원하면 유지로 바꿔도 됨)
            InvalidateRect(hWnd, NULL, FALSE);
            return 1;
        }
        g_selRow = row;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

// ---- app.c 호환용 ----
void Board_CreateControls(HWND hWnd) { g_hostWnd = hWnd; }
void Board_RelayoutControls(HWND hWnd) { g_hostWnd = hWnd; }
void Board_DestroyControls(void) { g_hostWnd = NULL; }
void Board_ShowControls(int show) { (void)show; }