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

	// ✅ 그리기/클릭
	void Board_ResetState(void);
	void Board_Draw(HDC hdc);
	int  Board_OnClick(HWND hWnd, int x, int y);

	// ✅ 선택된 게시물 얻기(수정/삭제/열기용으로 App에서 사용)
	int  Board_GetSelectedIndex(void);                 // 현재 페이지에서 선택된 row(0..9) / 없으면 -1
	int  Board_GetSelectedPostId(void);                // 선택된 게시물 id / 없으면 0
	int  Board_GetPostById(int id, wchar_t* outTitle, int tCap, wchar_t* outAuthor, int aCap, wchar_t* outContent, int cCap);

	// ---- app.c 호환용(링크 에러 방지용) ----
	void Board_CreateControls(HWND hWnd);
	void Board_RelayoutControls(HWND hWnd);
	void Board_DestroyControls(void);
	void Board_ShowControls(int show);

#ifdef __cplusplus
}
#endif