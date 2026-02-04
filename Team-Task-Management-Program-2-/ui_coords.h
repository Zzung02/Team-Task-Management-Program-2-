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
#define R_START_PW_X2   540
#define R_START_PW_Y2   545   

// =========================================================
// START 버튼들
// =========================================================
#define R_BTN_LOGIN_X1       355
#define R_BTN_LOGIN_Y1       544
#define R_BTN_LOGIN_X2       520
#define R_BTN_LOGIN_Y2       770

#define R_BTN_TO_SIGNUP_X1   903
#define R_BTN_TO_SIGNUP_Y1   528
#define R_BTN_TO_SIGNUP_X2   1143
#define R_BTN_TO_SIGNUP_Y2   588

#define R_BTN_FINDPW_X1      801
#define R_BTN_FINDPW_Y1      589
#define R_BTN_FINDPW_X2      1261
#define R_BTN_FINDPW_Y2      659

// =========================================================
// SIGNUP  회원가입 버튼/필드
// =========================================================
#define R_BTN_BACK_X1        265
#define R_BTN_BACK_Y1        40
#define R_BTN_BACK_X2        90
#define R_BTN_BACK_Y2        50

#define R_SIGN_NAME_X1       485
#define R_SIGN_NAME_Y1       205
#define R_SIGN_NAME_X2       800
#define R_SIGN_NAME_Y2       230

#define R_SIGN_ID_X1         436
#define R_SIGN_ID_Y1         288
#define R_SIGN_ID_X2         861
#define R_SIGN_ID_Y2         310

#define R_SIGN_PW_X1         450
#define R_SIGN_PW_Y1         365
#define R_SIGN_PW_X2         720
#define R_SIGN_PW_Y2         395

#define R_BTN_CREATE_X1      520
#define R_BTN_CREATE_Y1      388
#define R_BTN_CREATE_X2      700
#define R_BTN_CREATE_Y2      610

// =========================================================
// FINDPW(비밀번호 찾기) 텍스트필드들
// =========================================================
#define R_FIND_NAME_X1   160
#define R_FIND_NAME_Y1   215
#define R_FIND_NAME_X2   500
#define R_FIND_NAME_Y2   240

#define R_FIND_ID_X1     120
#define R_FIND_ID_Y1     300
#define R_FIND_ID_X2     546
#define R_FIND_ID_Y2     320

#define R_FIND_RESULT_X1  840
#define R_FIND_RESULT_Y1  150
#define R_FIND_RESULT_X2  1105
#define R_FIND_RESULT_Y2  300

// FINDPW "찾기" 버튼 
#define R_FIND_BTN_X1   363
#define R_FIND_BTN_Y1   375
#define R_FIND_BTN_X2   549
#define R_FIND_BTN_Y2   413



// =========================================================
// MAIN 왼쪽 필터 버튼
// =========================================================
#define R_MAIN_BTN_DEADLINE_X1  25
#define R_MAIN_BTN_DEADLINE_Y1  115
#define R_MAIN_BTN_DEADLINE_X2  115
#define R_MAIN_BTN_DEADLINE_Y2  190

#define R_MAIN_BTN_TODO_X1      34
#define R_MAIN_BTN_TODO_Y1      205
#define R_MAIN_BTN_TODO_X2      93
#define R_MAIN_BTN_TODO_Y2      257

#define R_MAIN_BTN_MYTEAM_X1    33
#define R_MAIN_BTN_MYTEAM_Y1    278
#define R_MAIN_BTN_MYTEAM_X2    98
#define R_MAIN_BTN_MYTEAM_Y2    329

#define R_MAIN_BTN_DONE_X1      30
#define R_MAIN_BTN_DONE_Y1      348
#define R_MAIN_BTN_DONE_X2      94
#define R_MAIN_BTN_DONE_Y2      327

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
#define R_TC_TEAM_X1   785
#define R_TC_TEAM_Y1   230
#define R_TC_TEAM_X2   1037
#define R_TC_TEAM_Y2   258

#define R_TC_TASK_X1   815
#define R_TC_TASK_Y1   304
#define R_TC_TASK_X2   1037
#define R_TC_TASK_Y2   333

#define R_TC_CODE_X1   853
#define R_TC_CODE_Y1   383
#define R_TC_CODE_X2   1030
#define R_TC_CODE_Y2   401

// TEAM_CREATE: 저장 버튼
#define R_TC_SAVE_X1  994
#define R_TC_SAVE_Y1  140
#define R_TC_SAVE_X2  1106
#define R_TC_SAVE_Y2  178

// MAIN - 내 팀 목록(ListBox) 영역(원본 디자인 기준)
#define R_MYTEAM_LIST_X1   160
#define R_MYTEAM_LIST_Y1   205
#define R_MYTEAM_LIST_X2   517
#define R_MYTEAM_LIST_Y2   658


// =========================================================
// TEAM_JOIN(팀참여) 필드
// =========================================================
#define R_TJ_TEAM_X1   792
#define R_TJ_TEAM_Y1   227
#define R_TJ_TEAM_X2   1038
#define R_TJ_TEAM_Y2   252

#define R_TJ_CODE_X1   852
#define R_TJ_CODE_Y1   304
#define R_TJ_CODE_X2   1038
#define R_TJ_CODE_Y2   330

#define R_TJ_SAVE_X1  995
#define R_TJ_SAVE_Y1  138
#define R_TJ_SAVE_X2  1105
#define R_TJ_SAVE_Y2  177





// =========================================================
// MAIN 상단 팀명/과제명 텍스트박스
// =========================================================
#define R_MAIN_TEAM_X1   211
#define R_MAIN_TEAM_Y1   79
#define R_MAIN_TEAM_X2   568
#define R_MAIN_TEAM_Y2   104

#define R_MAIN_TASK_X1   699
#define R_MAIN_TASK_Y1   78
#define R_MAIN_TASK_X2   1109
#define R_MAIN_TASK_Y2   95


// ===============================
// 과제 화면(SCR_TASK_ADD) Edit들
// ===============================
extern HWND g_edTaItem1;
extern HWND g_edTaItem2;
extern HWND g_edTaItem3;
extern HWND g_edTaItem4;

extern HWND g_edTaTitle;
extern HWND g_edTaDetail;
extern HWND g_edTaFile;



// =========================================================
// TASK_ADD 좌표 
// =========================================================
#define R_TA_ITEM1_X1  123
#define R_TA_ITEM1_Y1  253
#define R_TA_ITEM1_X2  544
#define R_TA_ITEM1_Y2  283

#define R_TA_ITEM2_X1  122
#define R_TA_ITEM2_Y1  328
#define R_TA_ITEM2_X2  547
#define R_TA_ITEM2_Y2  352

#define R_TA_ITEM3_X1  122
#define R_TA_ITEM3_Y1  405
#define R_TA_ITEM3_X2  548
#define R_TA_ITEM3_Y2  432

#define R_TA_ITEM4_X1  121
#define R_TA_ITEM4_Y1  484
#define R_TA_ITEM4_X2  546
#define R_TA_ITEM4_Y2  511

#define R_TA_TITLE_X1  788
#define R_TA_TITLE_Y1  188
#define R_TA_TITLE_X2  1185
#define R_TA_TITLE_Y2  217

#define R_TA_DETAIL_X1  789
#define R_TA_DETAIL_Y1  260
#define R_TA_DETAIL_X2  1178
#define R_TA_DETAIL_Y2  523


#define R_TA_SEARCH_X1  0   // 조회가 메인에 겹쳐보임
#define R_TA_SEARCH_Y1  0
#define R_TA_SEARCH_X2  0
#define R_TA_SEARCH_Y2  0


#define R_TA_FILE_X1  782
#define R_TA_FILE_Y1  601
#define R_TA_FILE_X2  951
#define R_TA_FILE_Y2  628


#define R_TA_SUBDETAIL_X1  787
#define R_TA_SUBDETAIL_Y1  541
#define R_TA_SUBDETAIL_X2  1151
#define R_TA_SUBDETAIL_Y2  564

/// ================================
// BOARD 
// ================================
#define R_BD_SEARCH_X1  696
#define R_BD_SEARCH_Y1  54
#define R_BD_SEARCH_X2  1171
#define R_BD_SEARCH_Y2  77

#define R_BD_TITLE_X1   178
#define R_BD_TITLE_Y1   182
#define R_BD_TITLE_X2   1002
#define R_BD_TITLE_Y2   204

#define R_BD_CONTENT_X1 176
#define R_BD_CONTENT_Y1 276
#define R_BD_CONTENT_X2 1182
#define R_BD_CONTENT_Y2 481

#define R_BD_BTN_ADD_X1   187
#define R_BD_BTN_ADD_Y1   39
#define R_BD_BTN_ADD_X2   378
#define R_BD_BTN_ADD_Y2   82

#define R_BD_BTN_EDIT_X1  404
#define R_BD_BTN_EDIT_Y1  60
#define R_BD_BTN_EDIT_X2  593
#define R_BD_BTN_EDIT_Y2  84

#define R_BD_BTN_DONE_X1  790
#define R_BD_BTN_DONE_Y1  60
#define R_BD_BTN_DONE_X2  1040
#define R_BD_BTN_DONE_Y2  120



//뒤로가기버튼
#define R_BACK_HIT_X1 1113   
#define R_BACK_HIT_Y1 21
#define R_BACK_HIT_X2 1301
#define R_BACK_HIT_Y2 53
