// team.c  (원샷 덮어쓰기)
#define _CRT_SECURE_NO_WARNINGS
#include "team.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

static int TrimLine(wchar_t* s) {
    if (!s) return 0;
    int n = (int)wcslen(s);
    while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) s[--n] = 0;
    return n;
}

static FILE* OpenTextUtf8(const wchar_t* path, const wchar_t* mode)
{
    FILE* fp = NULL;
    _wfopen_s(&fp, path, mode); // ex) L"r, ccs=UTF-8" / L"w, ccs=UTF-8" / L"a, ccs=UTF-8"
    if (fp) return fp;

    // fallback (ccs 옵션이 환경에 따라 실패할 수 있어서)
    // mode에서 ", ccs=UTF-8" 같은 부분이 있다면 제거한 모드로 재시도
    if (wcsstr(mode, L"ccs=")) {
        wchar_t m2[64] = { 0 };
        // 아주 단순 fallback: "r, ccs=UTF-8" -> "r"
        // "w, ccs=UTF-8" -> "w"
        // "a, ccs=UTF-8" -> "a"
        m2[0] = mode[0];
        m2[1] = 0;
        _wfopen_s(&fp, path, m2);
    }
    return fp;
}

// ---------------------------------------------------------
// ✅ teams.txt 라인 파싱: teamId|teamName|joinCode|ownerId
// ---------------------------------------------------------
static int ParseTeamLine(const wchar_t* line, TeamInfo* out)
{
    if (!line || !out) return 0;

    wchar_t tid[64] = L"", tname[128] = L"", jcode[64] = L"", owner[128] = L"";
    if (swscanf(line, L"%63[^|]|%127[^|]|%63[^|]|%127[^\r\n]", tid, tname, jcode, owner) != 4)
        return 0;

    ZeroMemory(out, sizeof(*out));
    lstrcpynW(out->teamId, tid, (int)_countof(out->teamId));
    lstrcpynW(out->teamName, tname, (int)_countof(out->teamName));
    lstrcpynW(out->joinCode, jcode, (int)_countof(out->joinCode));
    lstrcpynW(out->ownerUserId, owner, (int)_countof(out->ownerUserId));
    return 1;
}

// =========================================================
// ✅ team.h에 선언된 "기본 기능" 구현 4종
// =========================================================
BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam)
{
    if (!teamId || !teamId[0] || !outTeam) return FALSE;
    ZeroMemory(outTeam, sizeof(*outTeam));

    FILE* fp = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) return FALSE;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        TrimLine(line);

        TeamInfo t;
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

BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !outTeam) return FALSE;
    ZeroMemory(outTeam, sizeof(*outTeam));

    FILE* fp = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) return FALSE;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        TrimLine(line);

        TeamInfo t;
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

BOOL Team_IsOwner(const wchar_t* teamId, const wchar_t* userId)
{
    if (!teamId || !teamId[0] || !userId || !userId[0]) return FALSE;

    TeamInfo t = { 0 };
    if (!Team_FindByTeamId(teamId, &t)) return FALSE;

    return (wcscmp(t.ownerUserId, userId) == 0) ? TRUE : FALSE;
}

int Team_LoadMembers(const wchar_t* teamId, TeamMember* outArr, int maxCount)
{
    if (!teamId || !teamId[0] || !outArr || maxCount <= 0) return 0;

    FILE* fp = OpenTextUtf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!fp) return 0;

    int count = 0;
    wchar_t line[512];

    while (fgetws(line, 512, fp) && count < maxCount) {
        TrimLine(line);

        wchar_t tid[64] = L"", uid[128] = L"", role[32] = L"";
        if (swscanf(line, L"%63[^|]|%127[^|]|%31[^\r\n]", tid, uid, role) != 3) continue;
        if (wcscmp(tid, teamId) != 0) continue;

        ZeroMemory(&outArr[count], sizeof(outArr[count]));
        lstrcpynW(outArr[count].teamId, tid, (int)_countof(outArr[count].teamId));
        lstrcpynW(outArr[count].userId, uid, (int)_countof(outArr[count].userId));
        lstrcpynW(outArr[count].role, role, (int)_countof(outArr[count].role));
        count++;
    }

    fclose(fp);
    return count;
}

// =========================================================
// 기존 네 기능들 유지 (Team_GetOwnerId, 자동위임, 삭제, 위임)
// =========================================================
int Team_GetOwnerId(const wchar_t* teamId, wchar_t* outOwner, int outCch)
{
    if (!teamId || !teamId[0] || !outOwner || outCch <= 0) return 0;
    outOwner[0] = 0;

    FILE* fp = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        TrimLine(line);

        wchar_t tid[64], tname[128], jcode[64], owner[64];
        tid[0] = tname[0] = jcode[0] = owner[0] = 0;

        if (swscanf(line, L"%63[^|]|%127[^|]|%63[^|]|%63[^\n]", tid, tname, jcode, owner) == 4) {
            if (wcscmp(tid, teamId) == 0) {
                lstrcpynW(outOwner, owner, outCch);
                fclose(fp);
                return 1;
            }
        }
    }

    fclose(fp);
    return 0;
}

// ✅ (핵심) team_members.txt에서 "참여 순서 1등(팀장 제외)" 찾기
static int FindNextOwnerByJoinOrder(const wchar_t* teamId, const wchar_t* currentOwnerId,
    wchar_t* outNewOwner, int outCch)
{
    if (!teamId || !teamId[0] || !currentOwnerId || !currentOwnerId[0] || !outNewOwner || outCch <= 0)
        return 0;

    outNewOwner[0] = 0;

    FILE* fp = OpenTextUtf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp)) {
        TrimLine(line);

        wchar_t tid[64], uid[64], role[32];
        tid[0] = uid[0] = role[0] = 0;

        if (swscanf(line, L"%63[^|]|%63[^|]|%31[^\n]", tid, uid, role) == 3) {
            if (wcscmp(tid, teamId) != 0) continue;
            if (wcscmp(uid, currentOwnerId) == 0) continue; // 팀장 제외

            lstrcpynW(outNewOwner, uid, outCch);
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

static int RewriteTeamsOwner(const wchar_t* teamId, const wchar_t* newOwnerId)
{
    FILE* in = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!in) return 0;

    FILE* out = OpenTextUtf8(L"teams_tmp.txt", L"w, ccs=UTF-8");
    if (!out) { fclose(in); return 0; }

    wchar_t line[512];
    while (fgetws(line, 512, in)) {
        wchar_t buf[512];
        lstrcpynW(buf, line, 512);
        TrimLine(buf);

        wchar_t tid[64], tname[128], jcode[64], oid[64];
        tid[0] = tname[0] = jcode[0] = oid[0] = 0;

        if (swscanf(buf, L"%63[^|]|%127[^|]|%63[^|]|%63[^\n]", tid, tname, jcode, oid) == 4) {
            if (wcscmp(tid, teamId) == 0) {
                fwprintf(out, L"%s|%s|%s|%s\n", tid, tname, jcode, newOwnerId);
                continue;
            }
        }

        fputws(line, out);
    }

    fclose(in);
    fclose(out);

    _wremove(TEAMS_FILE);
    _wrename(L"teams_tmp.txt", TEAMS_FILE);
    return 1;
}

// ✅ team_members.txt에서 역할 교체(기존팀장->MEMBER, 새팀장->OWNER)
static int RewriteMembersSwapRole(const wchar_t* teamId, const wchar_t* oldOwnerId, const wchar_t* newOwnerId)
{
    FILE* in = OpenTextUtf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!in) return 0;

    FILE* out = OpenTextUtf8(L"team_members_tmp.txt", L"w, ccs=UTF-8");
    if (!out) { fclose(in); return 0; }

    wchar_t line[512];
    while (fgetws(line, 512, in)) {
        wchar_t buf[512];
        lstrcpynW(buf, line, 512);
        TrimLine(buf);

        wchar_t tid[64], uid[64], role[32];
        tid[0] = uid[0] = role[0] = 0;

        if (swscanf(buf, L"%63[^|]|%63[^|]|%31[^\n]", tid, uid, role) == 3) {
            if (wcscmp(tid, teamId) == 0) {
                if (wcscmp(uid, oldOwnerId) == 0) {
                    fwprintf(out, L"%s|%s|MEMBER\n", tid, uid);
                    continue;
                }
                if (wcscmp(uid, newOwnerId) == 0) {
                    fwprintf(out, L"%s|%s|OWNER\n", tid, uid);
                    continue;
                }
            }
        }

        fputws(line, out);
    }

    fclose(in);
    fclose(out);

    _wremove(TEAM_MEMBERS_FILE);
    _wrename(L"team_members_tmp.txt", TEAM_MEMBERS_FILE);
    return 1;
}

// ---------------------------------------------------------
// ✅ 자동 위임: "참여 순서"로 다음 팀장 선택
// ---------------------------------------------------------
int Team_TransferOwnerAuto(const wchar_t* teamId, const wchar_t* requesterId)
{
    if (!teamId || !teamId[0] || !requesterId || !requesterId[0]) return 0;

    wchar_t curOwner[64];
    if (!Team_GetOwnerId(teamId, curOwner, 64)) return 0;

    if (wcscmp(curOwner, requesterId) != 0) return 0; // 팀장만

    wchar_t newOwner[64];
    if (!FindNextOwnerByJoinOrder(teamId, curOwner, newOwner, 64)) return 0;

    if (!RewriteTeamsOwner(teamId, newOwner)) return 0;
    if (!RewriteMembersSwapRole(teamId, curOwner, newOwner)) return 0;

    return 1;
}

// ===============================
// 팀 삭제
// ===============================
BOOL Team_DeleteTeam(const wchar_t* teamId, const wchar_t* requestUserId)
{
    TeamInfo info = { 0 };

    if (!Team_FindByTeamId(teamId, &info))
        return FALSE;

    if (wcscmp(info.ownerUserId, requestUserId) != 0)
        return FALSE;

    FILE* in = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    FILE* out = OpenTextUtf8(L"teams_tmp.txt", L"w, ccs=UTF-8");

    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return FALSE;
    }

    wchar_t line[512];
    while (fgetws(line, 512, in))
    {
        wchar_t buf[512];
        lstrcpynW(buf, line, 512);
        TrimLine(buf);

        wchar_t tid[64] = L"";
        if (swscanf(buf, L"%63[^|]", tid) == 1) {
            if (wcscmp(tid, teamId) == 0) continue;
        }

        fputws(line, out);
    }

    fclose(in);
    fclose(out);

    _wremove(TEAMS_FILE);
    _wrename(L"teams_tmp.txt", TEAMS_FILE);

    return TRUE;
}

// ===============================
// 팀장 위임(수동)
// ===============================
BOOL Team_TransferOwner(const wchar_t* teamId,
    const wchar_t* requestUserId,
    const wchar_t* newOwnerUserId)
{
    TeamInfo info = { 0 };

    if (!Team_FindByTeamId(teamId, &info))
        return FALSE;

    if (wcscmp(info.ownerUserId, requestUserId) != 0)
        return FALSE;

    FILE* in = OpenTextUtf8(TEAMS_FILE, L"r, ccs=UTF-8");
    FILE* out = OpenTextUtf8(L"teams_tmp.txt", L"w, ccs=UTF-8");

    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return FALSE;
    }

    wchar_t line[512];
    while (fgetws(line, 512, in))
    {
        wchar_t buf[512];
        lstrcpynW(buf, line, 512);
        TrimLine(buf);

        wchar_t tid[64] = L"", tname[128] = L"", jcode[64] = L"", owner[128] = L"";
        if (swscanf(buf, L"%63[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, jcode, owner) == 4) {
            if (wcscmp(tid, teamId) == 0) {
                fwprintf(out, L"%s|%s|%s|%s\n", tid, tname, jcode, newOwnerUserId);
                continue;
            }
        }

        fputws(line, out);
    }

    fclose(in);
    fclose(out);

    _wremove(TEAMS_FILE);
    _wrename(L"teams_tmp.txt", TEAMS_FILE);

    return TRUE;
}

// ---------------------------------------------------------
// ✅ teamId 생성 (Tick 기반)
// ---------------------------------------------------------
static void GenTeamId(wchar_t* out, int cap)
{
    DWORD t = GetTickCount();
    swprintf(out, cap, L"T%08X", (unsigned)t);
}

// ---------------------------------------------------------
// ✅ Team_Create
//  - teams.txt에 teamId|teamName|joinCode|ownerId 추가
//  - team_members.txt에 teamId|ownerId|OWNER 추가
// ---------------------------------------------------------
BOOL Team_Create(const wchar_t* teamName, const wchar_t* joinCode,
    const wchar_t* ownerUserId, TeamInfo* outTeam)
{
    if (!teamName || !teamName[0] || !joinCode || !joinCode[0] ||
        !ownerUserId || !ownerUserId[0] || !outTeam) return FALSE;

    // joinCode 중복 체크
    TeamInfo tmp;
    if (Team_FindByJoinCode(joinCode, &tmp)) return FALSE;

    TeamInfo t;
    ZeroMemory(&t, sizeof(t));
    GenTeamId(t.teamId, (int)_countof(t.teamId));
    lstrcpynW(t.teamName, teamName, (int)_countof(t.teamName));
    lstrcpynW(t.joinCode, joinCode, (int)_countof(t.joinCode));
    lstrcpynW(t.ownerUserId, ownerUserId, (int)_countof(t.ownerUserId));

    // teams.txt append
    FILE* fp = OpenTextUtf8(TEAMS_FILE, L"a, ccs=UTF-8");
    if (!fp) return FALSE;
    fwprintf(fp, L"%s|%s|%s|%s\n", t.teamId, t.teamName, t.joinCode, t.ownerUserId);
    fclose(fp);

    // team_members.txt append: OWNER
    fp = OpenTextUtf8(TEAM_MEMBERS_FILE, L"a, ccs=UTF-8");
    if (fp) {
        fwprintf(fp, L"%s|%s|OWNER\n", t.teamId, ownerUserId);
        fclose(fp);
    }

    *outTeam = t;
    return TRUE;
}

// ---------------------------------------------------------
// ✅ Team_JoinByCode
//  - joinCode로 팀 찾기
//  - team_members.txt에 teamId|userId|MEMBER 추가 (중복 방지)
// ---------------------------------------------------------
BOOL Team_JoinByCode(const wchar_t* joinCode, const wchar_t* userId, TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !userId || !userId[0] || !outTeam) return FALSE;

    TeamInfo t;
    if (!Team_FindByJoinCode(joinCode, &t)) return FALSE;

    // 이미 가입했는지 확인
    FILE* fp = OpenTextUtf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (fp) {
        wchar_t line[512];
        while (fgetws(line, 512, fp)) {
            TrimLine(line);
            wchar_t tid[64] = L"", uid[128] = L"", role[32] = L"";
            if (swscanf(line, L"%63[^|]|%127[^|]|%31[^\r\n]", tid, uid, role) != 3) continue;
            if (wcscmp(tid, t.teamId) == 0 && wcscmp(uid, userId) == 0) {
                fclose(fp);
                *outTeam = t;
                return TRUE; // 이미 가입 -> 성공처리
            }
        }
        fclose(fp);
    }

    // append MEMBER
    fp = OpenTextUtf8(TEAM_MEMBERS_FILE, L"a, ccs=UTF-8");
    if (!fp) return FALSE;
    fwprintf(fp, L"%s|%s|MEMBER\n", t.teamId, userId);
    fclose(fp);

    *outTeam = t;
    return TRUE;
}

// ---------------------------------------------------------
// ✅ RoleToKorean
// ---------------------------------------------------------
const wchar_t* RoleToKorean(const wchar_t* role)
{
    if (!role) return L"";
    if (lstrcmpiW(role, L"OWNER") == 0) return L"팀장";
    if (lstrcmpiW(role, L"LEADER") == 0) return L"팀장";
    if (lstrcmpiW(role, L"MEMBER") == 0) return L"팀원";
    return role;
}