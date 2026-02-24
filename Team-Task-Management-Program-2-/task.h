#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_TITLE_MAX   128
#define TASK_TEXT_MAX    2048
#define TASK_FILE_MAX    260
#define TASK_DEADLINE_MAX  32

    typedef struct {
        int id;
        int done;
        wchar_t title[TASK_TITLE_MAX];
        wchar_t content[TASK_TEXT_MAX];
        wchar_t detail[TASK_TEXT_MAX];
        wchar_t file[TASK_FILE_MAX];
        wchar_t deadline[TASK_DEADLINE_MAX];
    } TaskItem;

    int  Task_LoadAll(const wchar_t* teamId, TaskItem* outArr, int maxCount);
    int  Task_SaveAll(const wchar_t* teamId, const TaskItem* arr, int count);

    int  Task_NextId(const TaskItem* arr, int count);
    int  Task_LoadActiveOnly(const wchar_t* teamId, TaskItem* out, int cap);

    int  Task_Add(const wchar_t* teamId, const TaskItem* item);
    int  Task_Update(const wchar_t* teamId, const TaskItem* item);
    int  Task_Delete(const wchar_t* teamId, int id);
    int  Task_SetDone(const wchar_t* teamId, int id, int done);
    int  Task_FindByTitle(const wchar_t* teamId, const wchar_t* keyword, TaskItem* outItem);

#ifdef __cplusplus
}
#endif