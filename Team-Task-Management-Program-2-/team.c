// team.c
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

static void rtrim_w(wchar_t* s)
{
    if (!s) return;
    int n = (int)wcslen(s);
    while (n > 0) {
        wchar_t c = s[n - 1];
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') {
            s[n - 1] = 0;
            n--;
        }
        else break;
    }
}

// =========================================================
// Team_Create : 팀 등록 (팀명/과제명/참여코드 저장)
// teams.txt : team|task|code
// =========================================================
int Team_Create(const wchar_t* team, const wchar_t* task, const wchar_t* code)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, L"teams.txt", L"a+, ccs=UTF-8");
    if (!fp) return 0;

    // 참여코드 중복 검사
    wchar_t line[512];
    wchar_t t[256], ta[256], c[256];

    fseek(fp, 0, SEEK_SET);
    while (fgetws(line, (int)_countof(line), fp)) {
        t[0] = ta[0] = c[0] = 0;

        int n = swscanf_s(line, L" %255[^|]| %255[^|]| %255[^\n]",
            t, (unsigned)_countof(t),
            ta, (unsigned)_countof(ta),
            c, (unsigned)_countof(c));

        if (n == 3) {
            rtrim_w(t); rtrim_w(ta); rtrim_w(c);
            if (wcscmp(c, code) == 0) {
                fclose(fp);
                return -1; // 코드 중복
            }
        }
    }

    fwprintf(fp, L"%ls|%ls|%ls\n", team, task, code);
    fclose(fp);
    return 1;
}

// =========================================================
// Team_AddMember : 내가 이 팀에 속함 저장
// team_members.txt : userId|joinCode
// =========================================================
int Team_AddMember(const wchar_t* userId, const wchar_t* code)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"a+, ccs=UTF-8");
    if (!fp) return 0;

    fwprintf(fp, L"%ls|%ls\n", userId, code);
    fclose(fp);
    return 1;
}

// =========================================================
// Team_GetMyTeams : 내가 속한 팀 코드들 가져오기
// =========================================================
int Team_GetMyTeams(const wchar_t* userId, wchar_t outCodes[][128], int max)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, L"team_members.txt", L"r, ccs=UTF-8");
    if (!fp) return 0;

    int count = 0;
    wchar_t line[256], uid[128], code[128];

    while (fgetws(line, (int)_countof(line), fp) && count < max) {
        uid[0] = code[0] = 0;

        int n = swscanf_s(line, L" %127[^|]| %127[^\n]",
            uid, (unsigned)_countof(uid),
            code, (unsigned)_countof(code));

        if (n == 2) {
            rtrim_w(uid); rtrim_w(code);
            if (wcscmp(uid, userId) == 0) {
                wcsncpy_s(outCodes[count++], 128, code, _TRUNCATE);
            }
        }
    }

    fclose(fp);
    return count;
}

// =========================================================
// Team_FindByCode : 참여코드로 팀/과제 찾기
// =========================================================
int Team_FindByCode(const wchar_t* joinCode,
    wchar_t* outTeam, int outTeamCap,
    wchar_t* outTask, int outTaskCap)
{
    if (!joinCode || !joinCode[0]) return 0;

    FILE* fp = NULL;
    _wfopen_s(&fp, L"teams.txt", L"r, ccs=UTF-8");
    if (!fp) {
        _wfopen_s(&fp, L"teams.txt", L"r");
        if (!fp) return 0;
    }

    wchar_t line[512];
    while (fgetws(line, (int)_countof(line), fp)) {
        wchar_t team[256] = { 0 }, task[256] = { 0 }, code[256] = { 0 };

        int n = swscanf_s(line, L" %255[^|]| %255[^|]| %255[^\n]",
            team, (unsigned)_countof(team),
            task, (unsigned)_countof(task),
            code, (unsigned)_countof(code));

        if (n != 3) continue;

        rtrim_w(team); rtrim_w(task); rtrim_w(code);

        if (wcscmp(code, joinCode) == 0) {
            if (outTeam && outTeamCap > 0) wcsncpy_s(outTeam, outTeamCap, team, _TRUNCATE);
            if (outTask && outTaskCap > 0) wcsncpy_s(outTask, outTaskCap, task, _TRUNCATE);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}
