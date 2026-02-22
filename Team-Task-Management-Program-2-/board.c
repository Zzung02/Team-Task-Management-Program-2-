// board.c  ✅ 직접 그리기 + "팀별 파일" 저장/로드 + 페이지(◀▶) 클릭 처리
#define _CRT_SECURE_NO_WARNINGS
#include "board.h"
#include "ui_coords.h"
#include "app.h"        // g_currentTeamId 쓸 수도 있어서 포함(없어도 빌드는 됨)
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

// ✅ 선택/페이지
static int g_selRow = -1;   // 0..9 (현재 보이는 줄)
static int g_page = 0;    // 0..

// ✅ 현재 팀(파일 분리용)
static wchar_t g_boardTeamId[64] = L"";

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

// ✅ 팀별 파일 경로
static void BuildBoardPath(const wchar_t* teamId, wchar_t* outPath, int cap)
{
    if (!outPath || cap <= 0) return;

    if (!teamId || !teamId[0]) {
        wcsncpy(outPath, L"board_common.txt", cap - 1);
        outPath[cap - 1] = 0;
        return;
    }

    // teamId는 보통 T000001 형식이라 파일명에 안전
    _snwprintf_s(outPath, cap, _TRUNCATE, L"board_%s.txt", teamId);
    outPath[cap - 1] = 0;
}

// ✅ '|' / '\n' / '\\' 때문에 파싱 깨지는 거 방지(3개 이상 안 들어가는 원인 중 하나)
// - \n  ->  \\n
// - \   ->  \\
// - |   ->  \\p   (pipe)
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

static int Board_NextId(void)
{
    int maxId = 0;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id > maxId) maxId = g_posts[i].id;
    }
    return maxId + 1;
}

void Board_Reload(void)
{
    g_postCount = 0;

    wchar_t path[260];
    BuildBoardPath(g_boardTeamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"r, ccs=UTF-8");
    if (!fp) {
        // 파일 없으면 그냥 비어있는 상태
        return;
    }

    wchar_t line[4096];

    while (fgetws(line, 4096, fp) && g_postCount < BD_MAX_POSTS) {
        TrimNL(line);
        if (line[0] == 0) continue;

        // 포맷: id|title|author|content  (각 필드는 Escape 되어 있음)
        // 안전하게 직접 split (필드 자체에 '|'는 \\p 로 저장되므로 split OK)
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

    // ✅ 페이지가 범위를 넘으면 당겨오기
    int maxPage = (g_postCount <= 0) ? 0 : ((g_postCount - 1) / BD_ROWS_PER_PAGE);
    if (g_page > maxPage) g_page = maxPage;

    // 선택 줄이 현재 페이지에 유효한지
    if (g_selRow >= BD_ROWS_PER_PAGE) g_selRow = -1;
}

int Board_AddPost(const wchar_t* title, const wchar_t* author, const wchar_t* content)
{
    if (!title || !title[0] || !author || !author[0]) return 0;

    wchar_t path[260];
    BuildBoardPath(g_boardTeamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"a, ccs=UTF-8");
    if (!fp) return 0;

    int id = Board_NextId();

    wchar_t t2[BD_TITLE_MAX * 2] = { 0 };
    wchar_t a2[BD_AUTHOR_MAX * 2] = { 0 };
    wchar_t c2[BD_CONTENT_MAX * 2] = { 0 };

    EscapeField(title, t2, (int)(sizeof(t2) / sizeof(t2[0])));
    EscapeField(author, a2, (int)(sizeof(a2) / sizeof(a2[0])));
    EscapeField(content ? content : L"", c2, (int)(sizeof(c2) / sizeof(c2[0])));

    // 포맷: id|title|author|content
    fwprintf(fp, L"%d|%s|%s|%s\n", id, t2, a2, c2);
    fclose(fp);

    Board_Reload();
    return 1;
}

// --------------------
// 선택/조회 helper (App에서 수정/삭제/열기용)
// --------------------
int Board_GetSelectedIndex(void) { return g_selRow; }

int Board_GetSelectedPostId(void)
{
    if (g_selRow < 0 || g_selRow >= BD_ROWS_PER_PAGE) return 0;
    int idx = g_page * BD_ROWS_PER_PAGE + g_selRow;
    if (idx < 0 || idx >= g_postCount) return 0;
    return g_posts[idx].id;
}

int Board_GetPostById(int id, wchar_t* outTitle, int tCap, wchar_t* outAuthor, int aCap, wchar_t* outContent, int cCap)
{
    if (id <= 0) return 0;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id == id) {
            if (outTitle && tCap > 0) { lstrcpynW(outTitle, g_posts[i].title, tCap); }
            if (outAuthor && aCap > 0) { lstrcpynW(outAuthor, g_posts[i].author, aCap); }
            if (outContent && cCap > 0) { lstrcpynW(outContent, g_posts[i].content, cCap); }
            return 1;
        }
    }
    return 0;
}

// --------------------
// draw
// --------------------
static void DrawCellText(HDC hdc, RECT rc, const wchar_t* text)
{
    RECT r = rc;
    r.left += 8;
    r.right -= 8;
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, text ? text : L"", -1, &r,
        DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

void Board_Draw(HDC hdc)
{
    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);

    int w = rcList.right - rcList.left;
    int h = rcList.bottom - rcList.top;
    if (w <= 0 || h <= 0) return;

    // 칼럼 폭
    int colNumW = (int)(w * 0.12);     // 번호
    int colAuthorW = (int)(w * 0.15);  // 글쓴이
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

    Rectangle(hdc, rcList.left, rcList.top, rcList.right, rcList.bottom);

    int start = g_page * BD_ROWS_PER_PAGE;

    for (int i = 0; i < BD_ROWS_PER_PAGE; i++)
    {
        int y1 = rcList.top + i * rowH;
        int y2 = (i == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        RECT rcNum = { xNumL, y1, xNumR, y2 };
        RECT rcTitle = { xTitleL, y1, xTitleR, y2 };
        RECT rcAuthor = { xAuthorL, y1, xAuthorR, y2 };

        Rectangle(hdc, rcNum.left, rcNum.top, rcNum.right, rcNum.bottom);
        Rectangle(hdc, rcTitle.left, rcTitle.top, rcTitle.right, rcTitle.bottom);
        Rectangle(hdc, rcAuthor.left, rcAuthor.top, rcAuthor.right, rcAuthor.bottom);

        int idx = start + i;

        wchar_t numBuf[32] = L"";
        wsprintfW(numBuf, L"%d", idx + 1);
        DrawCellText(hdc, rcNum, numBuf);

        if (idx < g_postCount) {
            DrawCellText(hdc, rcTitle, g_posts[idx].title);
            DrawCellText(hdc, rcAuthor, g_posts[idx].author);
        }
    }

    // 선택 테두리
    if (g_selRow >= 0 && g_selRow < BD_ROWS_PER_PAGE)
    {
        int y1 = rcList.top + g_selRow * rowH;
        int y2 = (g_selRow == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        HPEN penBold = CreatePen(PS_SOLID, 3, RGB(0, 0, 0));
        SelectObject(hdc, penBold);
        Rectangle(hdc, rcList.left + 1, y1 + 1, rcList.right - 1, y2 - 1);
        SelectObject(hdc, penThin);
        DeleteObject(penBold);
    }

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(penThin);
}

// --------------------
// click
// --------------------
int Board_OnClick(HWND hWnd, int x, int y)
{
    // ✅ 페이지 ◀
    if (PtInRc(RcScaled(R_BD_PAGE_PREV_X1, R_BD_PAGE_PREV_Y1, R_BD_PAGE_PREV_X2, R_BD_PAGE_PREV_Y2), x, y)) {
        if (g_page > 0) g_page--;
        g_selRow = -1;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    // ✅ 페이지 ▶
    if (PtInRc(RcScaled(R_BD_PAGE_NEXT_X1, R_BD_PAGE_NEXT_Y1, R_BD_PAGE_NEXT_X2, R_BD_PAGE_NEXT_Y2), x, y)) {
        int maxPage = (g_postCount <= 0) ? 0 : ((g_postCount - 1) / BD_ROWS_PER_PAGE);
        if (g_page < maxPage) g_page++;
        g_selRow = -1;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }

    // 목록 클릭(선택)
    RECT rcList = RcScaled(R_BOARD_LIST_X1, R_BOARD_LIST_Y1, R_BOARD_LIST_X2, R_BOARD_LIST_Y2);
    int row = HitRowIndex(x, y, rcList);
    if (row >= 0) {
        g_selRow = row;
        InvalidateRect(hWnd, NULL, FALSE);
        return 1;
    }
    return 0;
}

// ---- app.c가 아직 호출하는 함수들(링크 에러 방지용) ----
void Board_CreateControls(HWND hWnd) { (void)hWnd; }
void Board_RelayoutControls(HWND hWnd) { (void)hWnd; }
void Board_DestroyControls(void) {}
void Board_ShowControls(int show) { (void)show; }