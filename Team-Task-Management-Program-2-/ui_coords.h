//ui_coords.h

#pragma once

// 현재 창의 client 크기(app.c에서 관리)
extern int g_clientW;
extern int g_clientH;

// 기준(원본 BMP) 크기
#define BASE_W 1321
#define BASE_H 717

// ✅ 스케일 매크로 (현재 client 기준으로 자동 변환)
#define SX(x) ((int)((x) * (double)g_clientW / (double)BASE_W + 0.5))
#define SY(y) ((int)((y) * (double)g_clientH / (double)BASE_H + 0.5))

// =========================================================
// START 화면: ID/PW 입력 박스
// =========================================================
#define R_START_ID_X1   110
#define R_START_ID_Y1   440
#define R_START_ID_X2   540
#define R_START_ID_Y2   470

#define R_START_PW_X1   129
#define R_START_PW_Y1   515
#define R_START_PW_X2   536
#define R_START_PW_Y2   535 

// =========================================================
// START 버튼들
// =========================================================
#define R_BTN_LOGIN_X1       353
#define R_BTN_LOGIN_Y1       572
#define R_BTN_LOGIN_X2       543
#define R_BTN_LOGIN_Y2       612

#define R_BTN_TO_SIGNUP_X1   795
#define R_BTN_TO_SIGNUP_Y1   538
#define R_BTN_TO_SIGNUP_X2   1251
#define R_BTN_TO_SIGNUP_Y2   576

#define R_BTN_FINDPW_X1      794
#define R_BTN_FINDPW_Y1      607
#define R_BTN_FINDPW_X2      1250
#define R_BTN_FINDPW_Y2      644

// =========================================================
// SIGNUP  회원가입 버튼/필드
// =========================================================
#define R_BTN_BACK_X1        265
#define R_BTN_BACK_Y1        40
#define R_BTN_BACK_X2        90
#define R_BTN_BACK_Y2        50

#define R_SIGN_NAME_X1       270
#define R_SIGN_NAME_Y1       263
#define R_SIGN_NAME_X2       640
#define R_SIGN_NAME_Y2       284

#define R_SIGN_ID_X1         216
#define R_SIGN_ID_Y1         341
#define R_SIGN_ID_X2         645
#define R_SIGN_ID_Y2         366

#define R_SIGN_PW_X1         233
#define R_SIGN_PW_Y1         422
#define R_SIGN_PW_X2         645
#define R_SIGN_PW_Y2         444

#define R_BTN_CREATE_X1      453
#define R_BTN_CREATE_Y1      482
#define R_BTN_CREATE_X2      642
#define R_BTN_CREATE_Y2      525

// =========================================================
// FINDPW(비밀번호 찾기) 텍스트필드들
// =========================================================
#define R_FIND_NAME_X1   270
#define R_FIND_NAME_Y1   263
#define R_FIND_NAME_X2   638
#define R_FIND_NAME_Y2   283

#define R_FIND_ID_X1     215
#define R_FIND_ID_Y1     341
#define R_FIND_ID_X2     645
#define R_FIND_ID_Y2     365

#define R_FIND_RESULT_X1  895
#define R_FIND_RESULT_Y1  440
#define R_FIND_RESULT_X2  1095
#define R_FIND_RESULT_Y2  541

// FINDPW "찾기" 버튼 
#define R_FIND_BTN_X1   459
#define R_FIND_BTN_Y1   407
#define R_FIND_BTN_X2   646
#define R_FIND_BTN_Y2   446



// =========================================================
// MAIN 왼쪽 필터 버튼
// =========================================================


#define R_MAIN_URGENT_X1		33
#define R_MAIN_URGENT_Y1		134
#define R_MAIN_URGENT_X2		90
#define R_MAIN_URGENT_Y2		180


#define R_MAIN_BTN_TODO_X1      28
#define R_MAIN_BTN_TODO_Y1      206
#define R_MAIN_BTN_TODO_X2      100
#define R_MAIN_BTN_TODO_Y2      251

#define R_MAIN_BTN_DEADLINE_X1  34
#define R_MAIN_BTN_DEADLINE_Y1  128
#define R_MAIN_BTN_DEADLINE_X2  96
#define R_MAIN_BTN_DEADLINE_Y2  179

#define R_MAIN_TODO_X1          29
#define R_MAIN_TODO_Y1			203
#define R_MAIN_TODO_X2			92
#define R_MAIN_TODO_Y2			257

#define R_MAIN_BTN_MYTEAM_X1    37
#define R_MAIN_BTN_MYTEAM_Y1    278
#define R_MAIN_BTN_MYTEAM_X2    101
#define R_MAIN_BTN_MYTEAM_Y2    321

#define R_MAIN_BTN_DONE_X1      31
#define R_MAIN_BTN_DONE_Y1      350
#define R_MAIN_BTN_DONE_X2      97
#define R_MAIN_BTN_DONE_Y2      399 




// =========================================================
// MAIN 오른쪽 메뉴 버튼
// =========================================================
#define R_MAIN_BTN_TEAM_CREATE_X1  1149
#define R_MAIN_BTN_TEAM_CREATE_Y1  113
#define R_MAIN_BTN_TEAM_CREATE_X2  1265
#define R_MAIN_BTN_TEAM_CREATE_Y2  160

#define R_MAIN_BTN_TEAM_JOIN_X1    1150
#define R_MAIN_BTN_TEAM_JOIN_Y1    180
#define R_MAIN_BTN_TEAM_JOIN_X2    1265
#define R_MAIN_BTN_TEAM_JOIN_Y2    207

#define R_MAIN_BTN_TASK_ADD_X1     1151
#define R_MAIN_BTN_TASK_ADD_Y1     232
#define R_MAIN_BTN_TASK_ADD_X2     1264
#define R_MAIN_BTN_TASK_ADD_Y2     256

#define R_MAIN_BTN_BOARD_X1        1150
#define R_MAIN_BTN_BOARD_Y1        278
#define R_MAIN_BTN_BOARD_X2        1265
#define R_MAIN_BTN_BOARD_Y2        302


// =========================================================
// TEAM_CREATE(팀등록) 필드
// =========================================================
#define R_TC_TEAM_X1   787
#define R_TC_TEAM_Y1   242
#define R_TC_TEAM_X2   1025
#define R_TC_TEAM_Y2   266

#define R_TC_CODE_X1   848
#define R_TC_CODE_Y1   321
#define R_TC_CODE_X2   984
#define R_TC_CODE_Y2   345

// TEAM_CREATE: 저장 버튼
#define R_TC_SAVE_X1  944
#define R_TC_SAVE_Y1  151
#define R_TC_SAVE_X2  1106
#define R_TC_SAVE_Y2  193

// MAIN - 내 팀 목록(ListBox) 영역(원본 디자인 기준)
#define R_MYTEAM_LIST_X1   161
#define R_MYTEAM_LIST_Y1   201
#define R_MYTEAM_LIST_X2   520
#define R_MYTEAM_LIST_Y2  655

#define R_MYTEAM_SAVE_X1  427
#define R_MYTEAM_SAVE_Y1  143
#define R_MYTEAM_SAVE_X2  517
#define R_MYTEAM_SAVE_Y2  184



// =========================================================
// TEAM_JOIN(팀참여) 필드
// =========================================================
#define R_TJ_TEAM_X1   792
#define R_TJ_TEAM_Y1   236
#define R_TJ_TEAM_X2   1026
#define R_TJ_TEAM_Y2   262

#define R_TJ_CODE_X1   848
#define R_TJ_CODE_Y1   313
#define R_TJ_CODE_X2   1029
#define R_TJ_CODE_Y2   348

#define R_TJ_SAVE_X1  943
#define R_TJ_SAVE_Y1  153
#define R_TJ_SAVE_X2  1086
#define R_TJ_SAVE_Y2  189



// =========================================================
// MAIN 상단 팀명/
// =========================================================
#define R_MAIN_TEAM_X1   211
#define R_MAIN_TEAM_Y1   79
#define R_MAIN_TEAM_X2   568
#define R_MAIN_TEAM_Y2   104



// =========================================================
// TASK_ADD 좌표 
// =========================================================
// TASK_ADD 상단 버튼들(원본 좌표)
#define R_TA_BTN_ADD_X1   185
#define R_TA_BTN_ADD_Y1   41
#define R_TA_BTN_ADD_X2   303
#define R_TA_BTN_ADD_Y2   83

#define R_TA_BTN_EDIT_X1  342
#define R_TA_BTN_EDIT_Y1  39
#define R_TA_BTN_EDIT_X2  461
#define R_TA_BTN_EDIT_Y2  81

#define R_TA_BTN_DONE_X1  650
#define R_TA_BTN_DONE_Y1  39
#define R_TA_BTN_DONE_X2  808
#define R_TA_BTN_DONE_Y2  82

#define R_TA_SEARCH_ICON_X1  1225
#define R_TA_SEARCH_ICON_Y1  20
#define R_TA_SEARCH_ICON_X2  1285
#define R_TA_SEARCH_ICON_Y2  75

#define R_DEADLINE_LIST_X1 157
#define R_DEADLINE_LIST_Y1 202
#define R_DEADLINE_LIST_X2 517
#define R_DEADLINE_LIST_Y2 657

#define R_TODO_LIST_X1 157
#define R_TODO_LIST_Y1 202
#define R_TODO_LIST_X2 517
#define R_TODO_LIST_Y2 657

// 삭제 버튼(너 BMP에 있으면)
#define R_TA_BTN_DEL_X1  493
#define R_TA_BTN_DEL_Y1  40
#define R_TA_BTN_DEL_X2  612
#define R_TA_BTN_DEL_Y2  80

// DONE(완료목록)
#define R_DONE_LIST_X1   159
#define R_DONE_LIST_Y1   203
#define R_DONE_LIST_X2   514
#define R_DONE_LIST_Y2   655

//파일 X표시
#define R_TA_BTN_FILE_CLEAR_X1 964
#define R_TA_BTN_FILE_CLEAR_Y1 596
#define R_TA_BTN_FILE_CLEAR_X2 997
#define R_TA_BTN_FILE_CLEAR_Y2 619

#define R_TA_BTN_DOWNLOAD_X1 1020
#define R_TA_BTN_DOWNLOAD_Y1 593
#define R_TA_BTN_DOWNLOAD_X2 1173
#define R_TA_BTN_DOWNLOAD_Y2 621

// 페이지 좌/우 화살표(있으면)
#define R_TA_PAGE_PREV_X1 254
#define R_TA_PAGE_PREV_Y1 657
#define R_TA_PAGE_PREV_X2 278
#define R_TA_PAGE_PREV_Y2 686

#define R_TA_PAGE_NEXT_X1 329
#define R_TA_PAGE_NEXT_Y1 656
#define R_TA_PAGE_NEXT_X2 358
#define R_TA_PAGE_NEXT_Y2 686

//과제목록들 1~2
#define R_TA_ITEM1_X1  92
#define R_TA_ITEM1_Y1  253
#define R_TA_ITEM1_X2  544
#define R_TA_ITEM1_Y2  283

#define R_TA_ITEM2_X1  92
#define R_TA_ITEM2_Y1  328
#define R_TA_ITEM2_X2  547
#define R_TA_ITEM2_Y2  352

#define R_TA_ITEM3_X1  92
#define R_TA_ITEM3_Y1  405
#define R_TA_ITEM3_X2  548
#define R_TA_ITEM3_Y2  432

#define R_TA_ITEM4_X1  92
#define R_TA_ITEM4_Y1  484
#define R_TA_ITEM4_X2  546
#define R_TA_ITEM4_Y2  511

#define R_TA_TITLE_X1  761
#define R_TA_TITLE_Y1  186
#define R_TA_TITLE_X2  1162
#define R_TA_TITLE_Y2  203

// 조회 입력칸
#define R_TA_SEARCH_X1   912
#define R_TA_SEARCH_Y1   55
#define R_TA_SEARCH_X2   1224
#define R_TA_SEARCH_Y2   75

// 제목
#define R_TA_TITLE_X1    761
#define R_TA_TITLE_Y1    186
#define R_TA_TITLE_X2    1162
#define R_TA_TITLE_Y2    203

// 내용(멀티라인 큰 박스)
#define R_TA_CONTENT_X1  759
#define R_TA_CONTENT_Y1  250
#define R_TA_CONTENT_X2  1171
#define R_TA_CONTENT_Y2  528

// 상세사항(작은 줄)
#define R_TA_DETAIL_X1   785
#define R_TA_DETAIL_Y1   544
#define R_TA_DETAIL_X2   1014
#define R_TA_DETAIL_Y2   563

// 파일
#define R_TA_FILE_X1     754
#define R_TA_FILE_Y1     601
#define R_TA_FILE_X2     951
#define R_TA_FILE_Y2     628

#define R_TA_DEADLINE_X1   1073
#define R_TA_DEADLINE_Y1   542
#define R_TA_DEADLINE_X2   1171
#define R_TA_DEADLINE_Y2   559



// 목록 1페이지 표시 줄 수
#define BD_ROWS_PER_PAGE  10

// 페이지 화살표 (네 BMP 하단 화살표 기준으로 last click 잡아서 넣는게 베스트)
// 지금은 임시로 화면 중앙 아래쯤(너 스샷 기준) 잡아둠

// [SCR_BOARD] 목록 텍스트 박스(번호/제목/글쓴이 출력 영역)
#define R_BOARD_LIST_X1  91
#define R_BOARD_LIST_Y1  168
#define R_BOARD_LIST_X2  1223
#define R_BOARD_LIST_Y2  619


#define R_BD_PAGE_PREV_X1  610
#define R_BD_PAGE_PREV_Y1  615
#define R_BD_PAGE_PREV_X2  650
#define R_BD_PAGE_PREV_Y2  665

#define R_BD_PAGE_NEXT_X1  670
#define R_BD_PAGE_NEXT_Y1  615
#define R_BD_PAGE_NEXT_X2  710
#define R_BD_PAGE_NEXT_Y2  665



// ================================
// BOARD 목록 영역(번호/제목/글쓴이 텍스트가 올라갈 영역)
// ================================

//등록버튼
#define R_BOARD_BTN_REG_X1 188
#define R_BOARD_BTN_REG_Y1 41
#define R_BOARD_BTN_REG_X2 376
#define R_BOARD_BTN_REG_Y2 81

// (등록 화면) 저장 버튼(등록 완료)
#define R_BDW_SAVE_X1  182
#define R_BDW_SAVE_Y1  39
#define R_BDW_SAVE_X2  370
#define R_BDW_SAVE_Y2  83






// ================================
// CALENDAR (MAIN 화면에 그릴 달력 영역)
// ================================
#define R_CAL_GRID_X1   170
#define R_CAL_GRID_Y1   160
#define R_CAL_GRID_X2   1120
#define R_CAL_GRID_Y2   660

// 월 이동(세모 박스) - 너 스샷 왼쪽 위 세모 영역 기준
#define R_CAL_PREV_X1   84
#define R_CAL_PREV_Y1   58
#define R_CAL_PREV_X2   117
#define R_CAL_PREV_Y2   82

#define R_CAL_NEXT_X1   85
#define R_CAL_NEXT_Y1   92
#define R_CAL_NEXT_X2   116
#define R_CAL_NEXT_Y2   114

// "2026년 2월" 텍스트 표시 위치(세모 옆)
#define R_CAL_LABEL_X1  12
#define R_CAL_LABEL_Y1  54
#define R_CAL_LABEL_X2  75
#define R_CAL_LABEL_Y2  110

// 예시 (네가 last click으로 잡은 좌표로 채워)
#define R_CAL_X1  129
#define R_CAL_Y1  179
#define R_CAL_X2  1114
#define R_CAL_Y2  684

#define R_CAL_HEADER_H 60 

// =========================================================
// 공통 뒤로가기 버튼 (모든 화면)
// =========================================================
#define R_BTN_BACK_GLOBAL_X1   1152
#define R_BTN_BACK_GLOBAL_Y1   4
#define R_BTN_BACK_GLOBAL_X2   1308    // 버튼 크기는 적당히
#define R_BTN_BACK_GLOBAL_Y2   36



