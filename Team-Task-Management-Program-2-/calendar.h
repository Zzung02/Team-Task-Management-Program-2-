// calendar.h
#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

	// 캘린더 초기값(로그인 후 or 앱 시작 시 1회)
	void Calendar_Init(void);

	// 현재 표시 월(년/월) 설정 및 읽기
	void Calendar_SetYM(int year, int month);
	void Calendar_GetYM(int* outYear, int* outMonth);

	// 팀 id 지정(이벤트 재구성 포함)
	void Calendar_SetTeamId(const wchar_t* teamId);

	// 메인화면 캘린더 그리기 (App_OnPaint에서 호출)
	void Calendar_Draw(HDC hdc);

	// 메인화면 캘린더 클릭 처리 (◀▶/날짜칸 클릭 등)
	// - 클릭을 캘린더가 처리했으면 1 반환
	int  Calendar_OnClick(HWND hWnd, int x, int y);

	// 과제 파일을 다시 읽어서(팀 기준) 현재 월 이벤트(과제 제목)를 재구성
	void Calendar_RebuildFromTasks(const wchar_t* teamId);

	// 과제 변경(등록/수정/삭제/완료) 직후 호출용 유틸
	void Calendar_NotifyTasksChanged(HWND hWnd, const wchar_t* teamId);

	// ✅ clip mode
	// - mode 0: 전체 표시
	// - mode 1: 왼쪽 3칸(일/월/화) 가림 -> 수~토
	// - mode 2: 오른쪽 3칸(목/금/토) 가림 -> 일~수
	// - mode 3: 왼쪽 4칸(일/월/화/수) 가림 -> 목~토
	void Calendar_SetClipMode(int mode);

	// ✅ clipX: 이 X 이전은 캘린더를 아예 안 그림/클릭 안 됨
	void Calendar_SetClipX(int x);

	// ✅ 날짜 숫자 표시/숨김
	void Calendar_SetShowDayNumbers(int show);

	// ✅ (옵션) CompactMode = 날짜 숫자 숨기기 같은 용도로 쓰고 싶으면 여기서 처리
	void Calendar_SetCompactMode(int on);

	// ✅ 보이는 요일 마스크(원래 네가 원하던 API)
	// mask 비트: 0=일,1=월,2=화,3=수,4=목,5=금,6=토
	// 내부적으로 clipMode로 변환해줌
	void Calendar_SetVisibleWeekMask(unsigned int mask);

	void Calendar_SetClipRect(RECT rc);

#ifdef __cplusplus
}
#endif