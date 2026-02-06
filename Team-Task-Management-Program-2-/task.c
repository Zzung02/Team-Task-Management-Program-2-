// task.c
#define _CRT_SECURE_NO_WARNINGS
#include "task.h"

#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TASK_MAX_STORE  512

// ------------------------
// util
// ------------------------
static void TrimNL(wchar_t* s)
{
    if (!s) return;
    size_t n = wcslen(s);
    while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) {
        s[n - 1] = 0;
        n--;
    }
}

static void BuildTaskPath(const wchar_t* teamId, wchar_t* outPath, int cap)
{
    if (!outPath || cap <= 0) return;

    // teamId 없으면 공용
    if (!teamId || !teamId[0]) {
        wcsncpy(outPath, L"tasks_common.txt", cap - 1);
        outPath[cap - 1] = 0;
        return;
    }

    // ✅ 안전 포맷
    _snwprintf_s(outPath, cap, _TRUNCATE, L"tasks_%s.txt", teamId);
    outPath[cap - 1] = 0;
}

// 저장 포맷(1 task = 6 lines)
// id|done
// title
// content(escape된 한줄)
// detail(escape된 한줄)
// file
// --END--
static void EscapeToOneLine(const wchar_t* in, wchar_t* out, int cap)
{
    if (!out || cap <= 0) return;

    int oi = 0;
    for (int i = 0; in && in[i] && oi < cap - 1; i++) {
        if (in[i] == L'\r') continue;

        if (in[i] == L'\n') {
            if (oi < cap - 2) { out[oi++] = L'\\'; out[oi++] = L'n'; }
        }
        else if (in[i] == L'\\') {
            if (oi < cap - 2) { out[oi++] = L'\\'; out[oi++] = L'\\'; }
        }
        else {
            out[oi++] = in[i];
        }
    }
    out[oi] = 0;
}

static void UnescapeFromOneLine(const wchar_t* in, wchar_t* out, int cap)
{
    if (!out || cap <= 0) return;

    int oi = 0;
    for (int i = 0; in && in[i] && oi < cap - 1; i++) {
        if (in[i] == L'\\') {
            wchar_t n = in[i + 1];
            if (n == L'n') { out[oi++] = L'\n'; i++; continue; }
            if (n == L'\\') { out[oi++] = L'\\'; i++; continue; }
        }
        out[oi++] = in[i];
    }
    out[oi] = 0;
}

// ------------------------
// Load / Save
// ------------------------
int Task_LoadAll(const wchar_t* teamId, TaskItem* outArr, int maxCount)
{
    if (!outArr || maxCount <= 0) return 0;

    wchar_t path[260];
    BuildTaskPath(teamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"r, ccs=UTF-8");
    if (!fp) return 0;

    int count = 0;
    wchar_t line[4096];

    while (count < maxCount) {
        TaskItem t;
        ZeroMemory(&t, sizeof(t));

        // 1) id|done
        if (!fgetws(line, 4096, fp)) break;
        TrimNL(line);
        if (line[0] == 0) continue;

        swscanf(line, L"%d|%d", &t.id, &t.done);

        // 2) title
        if (!fgetws(line, 4096, fp)) break;
        TrimNL(line);
        wcsncpy(t.title, line, TASK_TITLE_MAX - 1);
        t.title[TASK_TITLE_MAX - 1] = 0;

        // 3) content
        if (!fgetws(line, 4096, fp)) break;
        TrimNL(line);
        UnescapeFromOneLine(line, t.content, TASK_TEXT_MAX);

        // 4) detail
        if (!fgetws(line, 4096, fp)) break;
        TrimNL(line);
        UnescapeFromOneLine(line, t.detail, TASK_TEXT_MAX);

        // 5) file
        if (!fgetws(line, 4096, fp)) break;
        TrimNL(line);
        wcsncpy(t.file, line, TASK_FILE_MAX - 1);
        t.file[TASK_FILE_MAX - 1] = 0;

        // 6) --END--
        if (!fgetws(line, 4096, fp)) break;

        outArr[count++] = t;
    }

    fclose(fp);
    return count;
}

int Task_SaveAll(const wchar_t* teamId, const TaskItem* arr, int count)
{
    if (!arr || count < 0) return 0;

    wchar_t path[260];
    BuildTaskPath(teamId, path, 260);

    FILE* fp = NULL;
    _wfopen_s(&fp, path, L"w, ccs=UTF-8");
    if (!fp) return 0;

    // ✅ 스택오버플로 방지: 큰 버퍼는 힙으로
    const int cap = TASK_TEXT_MAX * 2; // 4096 wchar_t
    wchar_t* c1 = (wchar_t*)calloc(cap, sizeof(wchar_t));
    wchar_t* d1 = (wchar_t*)calloc(cap, sizeof(wchar_t));
    if (!c1 || !d1) {
        if (c1) free(c1);
        if (d1) free(d1);
        fclose(fp);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        const TaskItem* t = &arr[i];

        c1[0] = 0;
        d1[0] = 0;

        EscapeToOneLine(t->content, c1, cap);
        EscapeToOneLine(t->detail, d1, cap);

        fwprintf(fp, L"%d|%d\n", t->id, t->done);
        fwprintf(fp, L"%ls\n", t->title);
        fwprintf(fp, L"%ls\n", c1);
        fwprintf(fp, L"%ls\n", d1);
        fwprintf(fp, L"%ls\n", t->file);
        fwprintf(fp, L"--END--\n");
    }

    free(c1);
    free(d1);
    fclose(fp);
    return 1;
}

int Task_NextId(const TaskItem* arr, int count)
{
    int maxId = 0;
    for (int i = 0; i < count; i++) {
        if (arr[i].id > maxId) maxId = arr[i].id;
    }
    return maxId + 1;
}

static int LoadToBuf(const wchar_t* teamId, TaskItem* buf, int cap)
{
    return Task_LoadAll(teamId, buf, cap);
}

// ------------------------
// CRUD
// ------------------------
int Task_Add(const wchar_t* teamId, const TaskItem* item)
{
    if (!item) return 0;

    TaskItem* buf = (TaskItem*)calloc(TASK_MAX_STORE, sizeof(TaskItem));
    if (!buf) return 0;

    int n = LoadToBuf(teamId, buf, TASK_MAX_STORE);

    TaskItem t = *item;
    t.id = Task_NextId(buf, n);

    int ok = 0;
    if (n < TASK_MAX_STORE) {
        buf[n++] = t;
        ok = Task_SaveAll(teamId, buf, n);
    }

    free(buf);
    return ok;
}

int Task_Update(const wchar_t* teamId, const TaskItem* item)
{
    if (!item) return 0;

    TaskItem* buf = (TaskItem*)calloc(TASK_MAX_STORE, sizeof(TaskItem));
    if (!buf) return 0;

    int n = LoadToBuf(teamId, buf, TASK_MAX_STORE);

    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i].id == item->id) {
            int keepDone = buf[i].done;
            buf[i] = *item;
            buf[i].done = keepDone; // 완료는 별도 버튼으로
            ok = Task_SaveAll(teamId, buf, n);
            break;
        }
    }

    free(buf);
    return ok;
}

int Task_Delete(const wchar_t* teamId, int id)
{
    TaskItem* buf = (TaskItem*)calloc(TASK_MAX_STORE, sizeof(TaskItem));
    if (!buf) return 0;

    int n = LoadToBuf(teamId, buf, TASK_MAX_STORE);

    int w = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i].id == id) continue;
        buf[w++] = buf[i];
    }

    int ok = Task_SaveAll(teamId, buf, w);
    free(buf);
    return ok;
}

int Task_SetDone(const wchar_t* teamId, int id, int done)
{
    TaskItem* buf = (TaskItem*)calloc(TASK_MAX_STORE, sizeof(TaskItem));
    if (!buf) return 0;

    int n = LoadToBuf(teamId, buf, TASK_MAX_STORE);

    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (buf[i].id == id) {
            buf[i].done = done ? 1 : 0;
            ok = Task_SaveAll(teamId, buf, n);
            break;
        }
    }

    free(buf);
    return ok;
}

int Task_FindByTitle(const wchar_t* teamId, const wchar_t* keyword, TaskItem* outItem)
{
    if (!keyword || !keyword[0] || !outItem) return 0;

    TaskItem* buf = (TaskItem*)calloc(TASK_MAX_STORE, sizeof(TaskItem));
    if (!buf) return 0;

    int n = LoadToBuf(teamId, buf, TASK_MAX_STORE);

    int found = 0;
    for (int i = 0; i < n; i++) {
        if (wcsstr(buf[i].title, keyword)) {
            *outItem = buf[i];
            found = 1;
            break;
        }
    }

    free(buf);
    return found;
}