// calendar.h
#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

	void Calendar_SetTeamId(const wchar_t* teamId);


	// 캘린더 초기값(로그인 후 or 앱 시작 시 1회)
	void Calendar_Init(void);

	// 현재 표시 월(년/월) 설정 및 읽기
	void Calendar_SetYM(int year, int month);
	void Calendar_GetYM(int* outYear, int* outMonth);

	// 메인화면 캘린더 그리기 (App_OnPaint에서 호출)
	void Calendar_Draw(HDC hdc);

	// 메인화면 캘린더 클릭 처리 (◀▶/날짜칸 클릭 등)
	// - 클릭을 캘린더가 처리했으면 1 반환(그럼 App_OnLButtonDown에서 SAFE_LEAVE하면 됨)
	int  Calendar_OnClick(HWND hWnd, int x, int y);

	// 과제 파일을 다시 읽어서(팀 기준) 현재 월 이벤트(과제 제목)를 재구성
	void Calendar_RebuildFromTasks(const wchar_t* teamId);

	// 과제 변경(등록/수정/삭제/완료) 직후 호출용 유틸
	void Calendar_NotifyTasksChanged(HWND hWnd, const wchar_t* teamId);

	void Calendar_SetClipMode(int mode);

	void Calendar_SetClipX(int x);   // ✅ 왼쪽 패널 끝 X (이 X 이전은 캘린더를 안 그림)

	void Calendar_SetShowDayNumbers(int show);
#ifdef __cplusplus
}
#endif