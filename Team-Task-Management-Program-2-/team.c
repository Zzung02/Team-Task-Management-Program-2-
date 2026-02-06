// team.c  (taskName 제거 + 구형 5칸 호환 파싱)
#define _CRT_SECURE_NO_WARNINGS
#include "team.h"
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>

static FILE* OpenUtf8Read(const wchar_t* path)
{
    FILE* fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) fp = _wfopen(path, L"r");
    return fp;
}

static FILE* OpenUtf8Append(const wchar_t* path)
{
    FILE* fp = _wfopen(path, L"a+, ccs=UTF-8");
    if (!fp) fp = _wfopen(path, L"a+");
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

// line: a|b|c
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

// line: a|b|c|d
static int Split4(const wchar_t* line,
    wchar_t* a, int an,
    wchar_t* b, int bn,
    wchar_t* c, int cn,
    wchar_t* d, int dn)
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

    if (!t1 || !t2 || !t3 || !t4) return 0;

    wcsncpy(a, t1, an - 1); a[an - 1] = 0;
    wcsncpy(b, t2, bn - 1); b[bn - 1] = 0;
    wcsncpy(c, t3, cn - 1); c[cn - 1] = 0;
    wcsncpy(d, t4, dn - 1); d[dn - 1] = 0;
    return 1;
}

// ✅ 호환 파서: (신형 4칸) + (구형 5칸) 모두 읽기
// 신형: teamId|teamName|joinCode|owner
// 구형: teamId|teamName|taskName|joinCode|owner   (taskName 무시)
static int ParseTeamLine(const wchar_t* line, TeamInfo* out)
{
    if (!line || !out) return 0;

    TeamInfo t = { 0 };

    // 1) 신형 4칸
    if (Split4(line,
        t.teamId, 32,
        t.teamName, 128,
        t.joinCode, 64,
        t.ownerUserId, 128))
    {
        *out = t;
        return 1;
    }

    // 2) 구형 5칸 (taskName 끼어있는 포맷)
    wchar_t buf[1024];
    wcsncpy(buf, line, 1023);
    buf[1023] = 0;
    TrimNewline(buf);

    wchar_t* ctx = NULL;
    wchar_t* a = wcstok(buf, L"|", &ctx);     // teamId
    wchar_t* b = wcstok(NULL, L"|", &ctx);    // teamName
    wchar_t* task = wcstok(NULL, L"|", &ctx); // taskName (버림)
    wchar_t* c = wcstok(NULL, L"|", &ctx);    // joinCode
    wchar_t* d = wcstok(NULL, L"|", &ctx);    // owner
    if (!a || !b || !task || !c || !d) return 0;

    wcsncpy(t.teamId, a, 31); t.teamId[31] = 0;
    wcsncpy(t.teamName, b, 127); t.teamName[127] = 0;
    wcsncpy(t.joinCode, c, 63); t.joinCode[63] = 0;
    wcsncpy(t.ownerUserId, d, 127); t.ownerUserId[127] = 0;

    *out = t;
    return 1;
}

static int JoinCodeExists(const wchar_t* joinCode)
{
    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (!fp) return 0;

    wchar_t line[1024];
    while (fgetws(line, 1024, fp)) {
        TeamInfo t = { 0 };
        if (!ParseTeamLine(line, &t)) continue;
        if (wcscmp(t.joinCode, joinCode) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void MakeNextTeamId(wchar_t* outId, int outLen)
{
    int maxNum = 0;

    FILE* fp = OpenUtf8Read(TEAMS_FILE);
    if (fp) {
        wchar_t line[1024];
        while (fgetws(line, 1024, fp)) {
            TeamInfo t = { 0 };
            if (!ParseTeamLine(line, &t)) continue;

            if (t.teamId[0] == L'T') {
                int num = _wtoi(t.teamId + 1);
                if (num > maxNum) maxNum = num;
            }
        }
        fclose(fp);
    }

    _snwprintf(outId, outLen, L"T%06d", maxNum + 1);
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
        if (!ParseTeamLine(line, &t)) continue;

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
        if (!ParseTeamLine(line, &t)) continue;

        if (wcscmp(t.teamId, teamId) == 0) {
            *outTeam = t;
            fclose(fp);
            return TRUE;
        }
    }
    fclose(fp);
    return FALSE;
}

// ✅ taskName 없이 생성
BOOL Team_Create(const wchar_t* teamName,
    const wchar_t* joinCode,
    const wchar_t* ownerUserId,
    TeamInfo* outTeam)
{
    if (!teamName || !teamName[0] ||
        !joinCode || !joinCode[0] ||
        !ownerUserId || !ownerUserId[0]) return FALSE;

    if (JoinCodeExists(joinCode)) return FALSE;

    TeamInfo t = { 0 };
    MakeNextTeamId(t.teamId, 32);
    wcsncpy(t.teamName, teamName, 127); t.teamName[127] = 0;
    wcsncpy(t.joinCode, joinCode, 63);  t.joinCode[63] = 0;
    wcsncpy(t.ownerUserId, ownerUserId, 127); t.ownerUserId[127] = 0;

    FILE* fp = OpenUtf8Append(TEAMS_FILE);
    if (!fp) return FALSE;

    // ✅ 4칸 저장 (teamId|teamName|joinCode|owner)
    fwprintf(fp, L"%s|%s|%s|%s\n",
        t.teamId, t.teamName, t.joinCode, t.ownerUserId);
    fclose(fp);

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

    if (MemberExists(t.teamId, userId)) return FALSE;

    FILE* fp = OpenUtf8Append(TEAM_MEMBERS_FILE);
    if (!fp) return FALSE;

    fwprintf(fp, L"%s|%s|%s\n", t.teamId, userId, L"MEMBER");
    fclose(fp);

    if (outTeam) *outTeam = t;
    return TRUE;
}