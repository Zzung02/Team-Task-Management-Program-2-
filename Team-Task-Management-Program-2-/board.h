// board.h
#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

    // ✅ 현재 팀 세팅(팀 바뀔 때 App에서 호출)
    void Board_SetTeamId(const wchar_t* teamId);

    // ✅ 데이터
    void Board_Reload(void);
    int  Board_AddPost(const wchar_t* title, const wchar_t* author, const wchar_t* content);

    // ✅ 수정/삭제 (파일까지 반영)
    int  Board_UpdatePost(int id, const wchar_t* newTitle, const wchar_t* newContent);
    int  Board_DeletePost(int id);

    // ✅ 그리기/클릭
    void Board_ResetState(void);
    void Board_Draw(HDC hdc);
    int  Board_OnClick(HWND hWnd, int x, int y);

    // ✅ 선택/조회
    int  Board_GetSelectedIndex(void);
    int  Board_GetSelectedPostId(void);
    int  Board_GetPostById(int id,
        wchar_t* outTitle, int tCap,
        wchar_t* outAuthor, int aCap,
        wchar_t* outContent, int cCap);

    // ---- app.c 호환용(링크 에러 방지용) ----
    void Board_CreateControls(HWND hWnd);
    void Board_RelayoutControls(HWND hWnd);
    void Board_DestroyControls(void);
    void Board_ShowControls(int show);

    // ✅ 조회(제목 검색: 완전일치)
   // board.h
    void Board_SetSearchQuery(const wchar_t* q);
    void Board_ApplySearch(void);
    void Board_ClearSearch(void);

#ifdef __cplusplus
}
#endif