// board.c  ✅ 직접 그리기 + 파일 저장/로드
#define _CRT_SECURE_NO_WARNINGS
#include "board.h"
#include "ui_coords.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#ifndef BD_ROWS_PER_PAGE
#define BD_ROWS_PER_PAGE 10
#endif

#define BOARD_FILE L"board_posts.txt"
#define BD_MAX_POSTS 512
#define BD_TITLE_MAX 128
#define BD_AUTHOR_MAX 128
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
static int g_selRow = -1;
static int g_page = 0;

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

// --------------------
// ✅ 파일 로드/저장
// --------------------
void Board_Reload(void)
{
    g_postCount = 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, BOARD_FILE, L"r, ccs=UTF-8");
    if (!fp) return;

    wchar_t line[4096];
    while (fgetws(line, 4096, fp) && g_postCount < BD_MAX_POSTS) {
        // id|title|author|content
        BoardPost p = { 0 };
        wchar_t title[BD_TITLE_MAX] = { 0 };
        wchar_t author[BD_AUTHOR_MAX] = { 0 };
        wchar_t content[BD_CONTENT_MAX] = { 0 };

        int id = 0;
        if (swscanf(line, L"%d|%127[^|]|%127[^|]|%2047[^\r\n]", &id, title, author, content) != 4)
            continue;

        p.id = id;
        lstrcpynW(p.title, title, BD_TITLE_MAX);
        lstrcpynW(p.author, author, BD_AUTHOR_MAX);
        lstrcpynW(p.content, content, BD_CONTENT_MAX);

        g_posts[g_postCount++] = p;
    }
    fclose(fp);

    if (g_selRow >= BD_ROWS_PER_PAGE) g_selRow = -1;
}

static int Board_NextId(void)
{
    int maxId = 0;
    for (int i = 0; i < g_postCount; i++) {
        if (g_posts[i].id > maxId) maxId = g_posts[i].id;
    }
    return maxId + 1;
}

int Board_AddPost(const wchar_t* title, const wchar_t* author, const wchar_t* content)
{
    if (!title || !title[0] || !author || !author[0]) return 0;

    // 파일 append
    FILE* fp = NULL;
    _wfopen_s(&fp, BOARD_FILE, L"a, ccs=UTF-8");
    if (!fp) return 0;

    int id = Board_NextId();

    // content에 줄바꿈 들어가면 한 줄 저장이 깨지니까 \n -> \\n 로 치환(간단 처리)
    wchar_t safeContent[BD_CONTENT_MAX] = { 0 };
    if (content && content[0]) {
        int w = 0;
        for (int i = 0; content[i] && w < BD_CONTENT_MAX - 2; i++) {
            if (content[i] == L'\r') continue;
            if (content[i] == L'\n') { safeContent[w++] = L'\\'; safeContent[w++] = L'n'; }
            else safeContent[w++] = content[i];
        }
        safeContent[w] = 0;
    }

    fwprintf(fp, L"%d|%s|%s|%s\n", id, title, author, safeContent);
    fclose(fp);

    Board_Reload();
    return 1;
}

void Board_ResetState(void)
{
    g_selRow = -1;
    g_page = 0;
}

// --------------------
// ✅ 그리기
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

    // ✅ 칼럼 폭 (원하는대로 더 줄여도 됨)
    int colNumW = (int)(w * 0.12);  // 번호
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

    // ✅ 현재 페이지 시작 인덱스
    int start = g_page * BD_ROWS_PER_PAGE;

    for (int i = 0; i < BD_ROWS_PER_PAGE; i++)
    {
        int y1 = rcList.top + i * rowH;
        int y2 = (i == BD_ROWS_PER_PAGE - 1) ? rcList.bottom : (y1 + rowH);

        RECT rcNum = { xNumL,    y1, xNumR,    y2 };
        RECT rcTitle = { xTitleL,  y1, xTitleR,  y2 };
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

int Board_OnClick(HWND hWnd, int x, int y)
{
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
