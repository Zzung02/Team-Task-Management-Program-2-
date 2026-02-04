//team.c

#define _CRT_SECURE_NO_WARNINGS
#include "team.h"
#include <stdio.h>
#include <wchar.h>

static FILE* OpenUtf8Read(const wchar_t* path)
{
    FILE* fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) fp = _wfopen(path, L"r"); // fallback
    return fp;
}

static FILE* OpenUtf8Append(const wchar_t* path)
{
    FILE* fp = _wfopen(path, L"a+, ccs=UTF-8");
    if (!fp) fp = _wfopen(path, L"a+"); // fallback
    return fp;
}

static void TrimNewline(wchar_t* s)
{
    if (!s) return;
    size_t n = wcslen(s);
    while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) {
        s[n - 1] = 0;
        n--;
    }
}

// line: a|b|c|d|e
static int Split5(const wchar_t* line,
    wchar_t* a, int an,
    wchar_t* b, int bn,
    wchar_t* c, int cn,
    wchar_t* d, int dn,
    wchar_t* e, int en)
{
    if (!line) return 0;

    wchar_t buf[1024];
    wcsncpy(buf, line, 1023);
    buf[1023] = 0;
    TrimNewline(buf);

    wchar_t* ctx = NULL;
    wchar_t* t1 = wcstok(buf, L"|", &ctx);
    wchar_t* t2 = wcstok(NULL, L"|", &ctx);
    wchar_t* t3 = wcstok(NULL, L"|", &ctx);
    wchar_t* t4 = wcstok(NULL, L"|", &ctx);
    wchar_t* t5 = wcstok(NULL, L"|", &ctx);

    if (!t1 || !t2 || !t3 || !t4 || !t5) return 0;

    wcsncpy(a, t1, an - 1); a[an - 1] = 0;
    wcsncpy(b, t2, bn - 1); b[bn - 1] = 0;
    wcsncpy(c, t3, cn - 1); c[cn - 1] = 0;
    wcsncpy(d, t4, dn - 1); d[dn - 1] = 0;
    wcsncpy(e, t5, en - 1); e[en - 1] = 0;
    return 1;
}

static int Split3(const wchar_t* line,
    wchar_t* a, int an,
    wchar_t* b, int bn,
    wchar_t* c, int cn)
{
    if (!line) return 0;

    wchar_t buf[512];
    wcsncpy(buf, line, 511);
    buf[511] = 0;
    TrimNewline(buf);

    wchar_t* ctx = NULL;
    wchar_t* t1 = wcstok(buf, L"|", &ctx);
    wchar_t* t2 = wcstok(NULL, L"|", &ctx);
    wchar_t* t3 = wcstok(NULL, L"|", &ctx);
    if (!t1 || !t2 || !t3) return 0;

    wcsncpy(a, t1, an - 1); a[an - 1] = 0;
    wcsncpy(b, t2, bn - 1); b[bn - 1] = 0;
    wcsncpy(c, t3, cn - 1); c[cn - 1] = 0;
    return 1;
}

static int JoinCodeExists(const wchar_t* joinCode)
{
    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (!fp) return 0; // 파일 없으면 아직 없음

    wchar_t line[1024];
    while (fgetws(line, 1024, fp)) {
        wchar_t id[32], tn[128], task[128], code[64], owner[128];
        if (!Split5(line, id, 32, tn, 128, task, 128, code, 64, owner, 128)) continue;
        if (wcscmp(code, joinCode) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void MakeNextTeamId(wchar_t* outId, int outLen)
{
    // T000001 같은 증가 아이디
    int maxNum = 0;

    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (fp) {
        wchar_t line[1024];
        while (fgetws(line, 1024, fp)) {
            wchar_t id[32], tn[128], task[128], code[64], owner[128];
            if (!Split5(line, id, 32, tn, 128, task, 128, code, 64, owner, 128)) continue;

            // id = "T000123"
            if (id[0] == L'T') {
                int num = _wtoi(id + 1);
                if (num > maxNum) maxNum = num;
            }
        }
        fclose(fp);
    }

    int next = maxNum + 1;
    _snwprintf(outId, outLen, L"T%06d", next);
    outId[outLen - 1] = 0;
}

static int MemberExists(const wchar_t* teamId, const wchar_t* userId)
{
    FILE* fp = OpenUtf8Read(TEAM_MEMBERS_FILE);
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        wchar_t tid[32], uid[128], role[32];
        if (!Split3(line, tid, 32, uid, 128, role, 32)) continue;
        if (wcscmp(tid, teamId) == 0 && wcscmp(uid, userId) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !outTeam) return FALSE;

    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (!fp) return FALSE;

    wchar_t line[1024];
    while (fgetws(line, 1024, fp)) {
        TeamInfo t = { 0 };
        if (!Split5(line,
            t.teamId, 32,
            t.teamName, 128,
            t.taskName, 128,
            t.joinCode, 64,
            t.ownerUserId, 128)) continue;

        if (wcscmp(t.joinCode, joinCode) == 0) {
            *outTeam = t;
            fclose(fp);
            return TRUE;
        }
    }
    fclose(fp);
    return FALSE;
}

BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam)
{
    if (!teamId || !teamId[0] || !outTeam) return FALSE;

    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (!fp) return FALSE;

    wchar_t line[1024];
    while (fgetws(line, 1024, fp)) {
        TeamInfo t = { 0 };
        if (!Split5(line,
            t.teamId, 32,
            t.teamName, 128,
            t.taskName, 128,
            t.joinCode, 64,
            t.ownerUserId, 128)) continue;

        if (wcscmp(t.teamId, teamId) == 0) {
            *outTeam = t;
            fclose(fp);
            return TRUE;
        }
    }
    fclose(fp);
    return FALSE;
}

BOOL Team_Create(const wchar_t* teamName,
    const wchar_t* taskName,
    const wchar_t* joinCode,
    const wchar_t* ownerUserId,
    TeamInfo* outTeam)
{
    if (!teamName || !teamName[0] ||
        !taskName || !taskName[0] ||
        !joinCode || !joinCode[0] ||
        !ownerUserId || !ownerUserId[0]) return FALSE;

    // ✅ 참여코드 중복 방지
    if (JoinCodeExists(joinCode)) return FALSE;

    TeamInfo t = { 0 };
    MakeNextTeamId(t.teamId, 32);
    wcsncpy(t.teamName, teamName, 127); t.teamName[127] = 0;
    wcsncpy(t.taskName, taskName, 127); t.taskName[127] = 0;
    wcsncpy(t.joinCode, joinCode, 63);  t.joinCode[63] = 0;
    wcsncpy(t.ownerUserId, ownerUserId, 127); t.ownerUserId[127] = 0;

    // ✅ teams.txt에 저장
    FILE* fp = OpenUtf8Append(TEAMS_FILE);
    if (!fp) return FALSE;

    fwprintf(fp, L"%s|%s|%s|%s|%s\n",
        t.teamId, t.teamName, t.taskName, t.joinCode, t.ownerUserId);
    fclose(fp);

    // ✅ 팀장도 멤버로 저장(OWNER)
    fp = OpenUtf8Append(TEAM_MEMBERS_FILE);
    if (!fp) return FALSE;

    fwprintf(fp, L"%s|%s|%s\n", t.teamId, ownerUserId, L"OWNER");
    fclose(fp);

    if (outTeam) *outTeam = t;
    return TRUE;
}

BOOL Team_JoinByCode(const wchar_t* joinCode,
    const wchar_t* userId,
    TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !userId || !userId[0]) return FALSE;

    TeamInfo t = { 0 };
    if (!Team_FindByJoinCode(joinCode, &t)) return FALSE;

    // ✅ 이미 가입했으면 실패
    if (MemberExists(t.teamId, userId)) return FALSE;

    FILE* fp = OpenUtf8Append(TEAM_MEMBERS_FILE);
    if (!fp) return FALSE;

    fwprintf(fp, L"%s|%s|%s\n", t.teamId, userId, L"MEMBER");
    fclose(fp);

    if (outTeam) *outTeam = t;
    return TRUE;
}