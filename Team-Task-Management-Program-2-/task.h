// task.h
#pragma once
#include <windows.h>

#define TASK_TITLE_MAX   128
#define TASK_TEXT_MAX    2048
#define TASK_FILE_MAX    260

typedef struct TaskItem {
    int     id;                 // 내부 식별자(자동 증가)
    int     done;               // 0/1
    wchar_t title[TASK_TITLE_MAX];
    wchar_t content[TASK_TEXT_MAX];
    wchar_t detail[TASK_TEXT_MAX];
    wchar_t file[TASK_FILE_MAX]; // URL 또는 attachments\...\xxx.ext
} TaskItem;

int  Task_LoadAll(const wchar_t* teamId, TaskItem* outArr, int maxCount);
int  Task_SaveAll(const wchar_t* teamId, const TaskItem* arr, int count);

int  Task_NextId(const TaskItem* arr, int count);

int  Task_Add(const wchar_t* teamId, const TaskItem* item);                 // 추가
int  Task_Update(const wchar_t* teamId, const TaskItem* item);              // id 기준 수정
int  Task_Delete(const wchar_t* teamId, int id);                            // 삭제
int  Task_SetDone(const wchar_t* teamId, int id, int done);                 // 완료 토글
int  Task_FindByTitle(const wchar_t* teamId, const wchar_t* keyword, TaskItem* outItem); // 조회(제목 포함)
