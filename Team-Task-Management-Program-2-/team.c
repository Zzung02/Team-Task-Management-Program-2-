// team.c  (원샷 덮어쓰기)  ✅ LNK2019 해결 + 팀 생성/참여/조회/삭제/위임/자동위임
#define _CRT_SECURE_NO_WARNINGS
#include "team.h"
#include <windows.h>
#include <wchar.h>
#include <stdio.h>

#ifndef TEAMS_FILE
#define TEAMS_FILE L"teams.txt"
#endif
#ifndef TEAM_MEMBERS_FILE
#define TEAM_MEMBERS_FILE L"team_members.txt"
#endif

// -------------------------------------------------
// 내부 유틸
// -------------------------------------------------
static FILE* wfopen_utf8(const wchar_t* path, const wchar_t* mode)
{
    FILE* fp = _wfopen(path, mode);
    if (!fp) {
        // fallback
        fp = _wfopen(path, mode[0] == L'r' ? L"r" : L"w");
    }
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

// teams.txt에서 teamId로 찾기
BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam)
{
    if (!teamId || !teamId[0] || !outTeam) return FALSE;
    ZeroMemory(outTeam, sizeof(*outTeam));

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (!fp) return FALSE;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, code[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, code, owner) == 4)
        {
            if (wcscmp(tid, teamId) == 0) {
                lstrcpynW(outTeam->teamId, tid, 32);
                lstrcpynW(outTeam->teamName, tname, 128);
                lstrcpynW(outTeam->joinCode, code, 64);
                lstrcpynW(outTeam->ownerUserId, owner, 128);
                fclose(fp);
                return TRUE;
            }
        }
    }

    fclose(fp);
    return FALSE;
}

// teams.txt에서 joinCode로 찾기
BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !outTeam) return FALSE;
    ZeroMemory(outTeam, sizeof(*outTeam));

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (!fp) return FALSE;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, code[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, code, owner) == 4)
        {
            if (wcscmp(code, joinCode) == 0) {
                lstrcpynW(outTeam->teamId, tid, 32);
                lstrcpynW(outTeam->teamName, tname, 128);
                lstrcpynW(outTeam->joinCode, code, 64);
                lstrcpynW(outTeam->ownerUserId, owner, 128);
                fclose(fp);
                return TRUE;
            }
        }
    }

    fclose(fp);
    return FALSE;
}

BOOL Team_IsOwner(const wchar_t* teamId, const wchar_t* userId)
{
    TeamInfo t;
    if (!Team_FindByTeamId(teamId, &t)) return FALSE;
    return (userId && userId[0] && wcscmp(t.ownerUserId, userId) == 0) ? TRUE : FALSE;
}

// team_members.txt 읽기
int Team_LoadMembers(const wchar_t* teamId, TeamMember* outArr, int maxCount)
{
    if (!teamId || !teamId[0] || !outArr || maxCount <= 0) return 0;

    FILE* fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");
    if (!fp) return 0;

    int n = 0;
    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%31[^\n]", tid, uid, role) == 3)
        {
            if (wcscmp(tid, teamId) != 0) continue;
            if (n >= maxCount) break;

            lstrcpynW(outArr[n].teamId, tid, 32);
            lstrcpynW(outArr[n].userId, uid, 128);
            lstrcpynW(outArr[n].role, role, 32);
            n++;
        }
    }
    fclose(fp);
    return n;
}

const wchar_t* RoleToKorean(const wchar_t* role)
{
    if (!role) return L"";
    if (wcscmp(role, L"OWNER") == 0) return L"팀장";
    return L"팀원";
}

// --------------------------------------------
// 중복 체크
// --------------------------------------------
int Team_ExistsByName(const wchar_t* teamName)
{
    if (!teamName || !teamName[0]) return 0;

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, code[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, code, owner) == 4)
        {
            if (wcscmp(tname, teamName) == 0) { fclose(fp); return 1; }
        }
    }
    fclose(fp);
    return 0;
}

int Team_ExistsByCode(const wchar_t* code)
{
    if (!code || !code[0]) return 0;

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, c[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, c, owner) == 4)
        {
            if (wcscmp(c, code) == 0) { fclose(fp); return 1; }
        }
    }
    fclose(fp);
    return 0;
}

// 팀명+코드가 일치하는지(참여 시)
int Team_CheckJoinCode(const wchar_t* teamName, const wchar_t* joinCode)
{
    if (!teamName || !teamName[0] || !joinCode || !joinCode[0]) return 0;

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, code[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, code, owner) == 4)
        {
            if (wcscmp(tname, teamName) == 0 && wcscmp(code, joinCode) == 0) {
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}

// --------------------------------------------
// teamId 생성: T0001, T0002 ...
// --------------------------------------------
static void MakeNextTeamId(wchar_t outId[32])
{
    int maxNo = 0;

    FILE* fp = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"r");
    if (fp)
    {
        wchar_t line[512];
        while (fgetws(line, 512, fp))
        {
            wchar_t tid[32] = { 0 };
            TrimNewline(line);
            if (swscanf(line, L"%31[^|]|", tid) >= 1)
            {
                // tid가 "T1234" 형태면 숫자 추출
                if (tid[0] == L'T') {
                    int n = _wtoi(tid + 1);
                    if (n > maxNo) maxNo = n;
                }
            }
        }
        fclose(fp);
    }

    wsprintfW(outId, L"T%04d", maxNo + 1);
}

// --------------------------------------------
// 팀 생성
// --------------------------------------------
BOOL Team_Create(const wchar_t* teamName, const wchar_t* joinCode,
    const wchar_t* ownerUserId, TeamInfo* outTeam)
{
    if (!teamName || !teamName[0] || !joinCode || !joinCode[0] || !ownerUserId || !ownerUserId[0])
        return FALSE;

    if (Team_ExistsByName(teamName)) return FALSE;
    if (Team_ExistsByCode(joinCode)) return FALSE;

    wchar_t teamId[32] = { 0 };
    MakeNextTeamId(teamId);

    // teams.txt append
    {
        FILE* fp = wfopen_utf8(TEAMS_FILE, L"a, ccs=UTF-8");
        if (!fp) fp = wfopen_utf8(TEAMS_FILE, L"a");
        if (!fp) return FALSE;

        fwprintf(fp, L"%s|%s|%s|%s\n", teamId, teamName, joinCode, ownerUserId);
        fclose(fp);
    }

    // team_members.txt append (OWNER)
    {
        FILE* fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"a, ccs=UTF-8");
        if (!fp) fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"a");
        if (!fp) return FALSE;

        fwprintf(fp, L"%s|%s|%s\n", teamId, ownerUserId, L"OWNER");
        fclose(fp);
    }

    if (outTeam) {
        ZeroMemory(outTeam, sizeof(*outTeam));
        lstrcpynW(outTeam->teamId, teamId, 32);
        lstrcpynW(outTeam->teamName, teamName, 128);
        lstrcpynW(outTeam->joinCode, joinCode, 64);
        lstrcpynW(outTeam->ownerUserId, ownerUserId, 128);
    }
    return TRUE;
}

// --------------------------------------------
// 참여: joinCode로 팀 찾고 멤버 추가
// --------------------------------------------
static int Member_AlreadyInTeam(const wchar_t* teamId, const wchar_t* userId)
{
    FILE* fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");
    if (!fp) return 0;

    wchar_t line[512];
    while (fgetws(line, 512, fp))
    {
        wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%31[^\n]", tid, uid, role) == 3)
        {
            if (wcscmp(tid, teamId) == 0 && wcscmp(uid, userId) == 0) {
                fclose(fp);
                return 1;
            }
        }
    }
    fclose(fp);
    return 0;
}

BOOL Team_JoinByCode(const wchar_t* joinCode, const wchar_t* userId, TeamInfo* outTeam)
{
    if (!joinCode || !joinCode[0] || !userId || !userId[0]) return FALSE;

    TeamInfo t;
    if (!Team_FindByJoinCode(joinCode, &t)) return FALSE;

    if (Member_AlreadyInTeam(t.teamId, userId)) {
        if (outTeam) *outTeam = t;
        return TRUE; // 이미 멤버면 성공 처리
    }

    FILE* fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"a, ccs=UTF-8");
    if (!fp) fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"a");
    if (!fp) return FALSE;

    fwprintf(fp, L"%s|%s|%s\n", t.teamId, userId, L"MEMBER");
    fclose(fp);

    if (outTeam) *outTeam = t;
    return TRUE;
}

// --------------------------------------------
// 팀 삭제: 팀장만 가능
// --------------------------------------------
static int DeleteTeamHard(const wchar_t* teamId)
{
    // teams.txt에서 제거
    {
        FILE* in = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
        if (!in) in = wfopen_utf8(TEAMS_FILE, L"r");
        if (!in) return 0;

        FILE* out = wfopen_utf8(L"teams_tmp.txt", L"w, ccs=UTF-8");
        if (!out) out = wfopen_utf8(L"teams_tmp.txt", L"w");
        if (!out) { fclose(in); return 0; }

        wchar_t line[512];
        while (fgetws(line, 512, in))
        {
            wchar_t tid[32] = { 0 };
            TrimNewline(line);
            if (swscanf(line, L"%31[^|]|", tid) >= 1) {
                if (wcscmp(tid, teamId) == 0) continue;
            }
            fputws(line, out);
            fputwc(L'\n', out);
        }
        fclose(in);
        fclose(out);

        _wremove(TEAMS_FILE);
        _wrename(L"teams_tmp.txt", TEAMS_FILE);
    }

    // team_members.txt에서 제거
    {
        FILE* in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
        if (!in) in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");
        if (!in) return 1;

        FILE* out = wfopen_utf8(L"team_members_tmp.txt", L"w, ccs=UTF-8");
        if (!out) out = wfopen_utf8(L"team_members_tmp.txt", L"w");
        if (!out) { fclose(in); return 1; }

        wchar_t line[512];
        while (fgetws(line, 512, in))
        {
            wchar_t tid[32] = { 0 };
            TrimNewline(line);
            if (swscanf(line, L"%31[^|]|", tid) >= 1) {
                if (wcscmp(tid, teamId) == 0) continue;
            }
            fputws(line, out);
            fputwc(L'\n', out);
        }
        fclose(in);
        fclose(out);

        _wremove(TEAM_MEMBERS_FILE);
        _wrename(L"team_members_tmp.txt", TEAM_MEMBERS_FILE);
    }
    return 1;
}

BOOL Team_DeleteTeam(const wchar_t* teamId, const wchar_t* requestUserId)
{
    if (!Team_IsOwner(teamId, requestUserId)) return FALSE;
    return DeleteTeamHard(teamId) ? TRUE : FALSE;
}

// --------------------------------------------
// 팀장 위임(수동)
// --------------------------------------------
static int UpdateTeamsOwner(const wchar_t* teamId, const wchar_t* newOwnerId)
{
    FILE* in = wfopen_utf8(TEAMS_FILE, L"r, ccs=UTF-8");
    if (!in) in = wfopen_utf8(TEAMS_FILE, L"r");
    if (!in) return 0;

    FILE* out = wfopen_utf8(L"teams_tmp.txt", L"w, ccs=UTF-8");
    if (!out) out = wfopen_utf8(L"teams_tmp.txt", L"w");
    if (!out) { fclose(in); return 0; }

    int updated = 0;
    wchar_t line[512];
    while (fgetws(line, 512, in))
    {
        wchar_t tid[32] = { 0 }, tname[128] = { 0 }, code[64] = { 0 }, owner[128] = { 0 };
        TrimNewline(line);
        if (swscanf(line, L"%31[^|]|%127[^|]|%63[^|]|%127[^\n]", tid, tname, code, owner) == 4)
        {
            if (wcscmp(tid, teamId) == 0) {
                fwprintf(out, L"%s|%s|%s|%s\n", tid, tname, code, newOwnerId);
                updated = 1;
                continue;
            }
        }
        fputws(line, out);
        fputwc(L'\n', out);
    }

    fclose(in);
    fclose(out);

    if (!updated) { _wremove(L"teams_tmp.txt"); return 0; }

    _wremove(TEAMS_FILE);
    _wrename(L"teams_tmp.txt", TEAMS_FILE);
    return 1;
}

static int RewriteMembersForTransfer(const wchar_t* teamId, const wchar_t* oldOwner, const wchar_t* newOwner)
{
    FILE* in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
    if (!in) in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");
    if (!in) return 0;

    FILE* out = wfopen_utf8(L"team_members_tmp.txt", L"w, ccs=UTF-8");
    if (!out) out = wfopen_utf8(L"team_members_tmp.txt", L"w");
    if (!out) { fclose(in); return 0; }

    wchar_t line[512];
    while (fgetws(line, 512, in))
    {
        wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
        TrimNewline(line);

        if (swscanf(line, L"%31[^|]|%127[^|]|%31[^\n]", tid, uid, role) == 3)
        {
            if (wcscmp(tid, teamId) != 0) {
                fwprintf(out, L"%s|%s|%s\n", tid, uid, role);
                continue;
            }

            if (wcscmp(uid, newOwner) == 0) {
                fwprintf(out, L"%s|%s|%s\n", tid, uid, L"OWNER");
                continue;
            }
            if (wcscmp(uid, oldOwner) == 0) {
                fwprintf(out, L"%s|%s|%s\n", tid, uid, L"MEMBER");
                continue;
            }

            fwprintf(out, L"%s|%s|%s\n", tid, uid, role);
            continue;
        }

        // 파싱 실패는 그대로 패스(안전)
    }

    fclose(in);
    fclose(out);
    _wremove(TEAM_MEMBERS_FILE);
    _wrename(L"team_members_tmp.txt", TEAM_MEMBERS_FILE);
    return 1;
}

BOOL Team_TransferOwner(const wchar_t* teamId,
    const wchar_t* requestUserId,
    const wchar_t* newOwnerUserId)
{
    if (!teamId || !teamId[0] || !requestUserId || !requestUserId[0] || !newOwnerUserId || !newOwnerUserId[0])
        return FALSE;

    TeamInfo t;
    if (!Team_FindByTeamId(teamId, &t)) return FALSE;
    if (wcscmp(t.ownerUserId, requestUserId) != 0) return FALSE; // 요청자가 팀장인지
    if (!Member_AlreadyInTeam(teamId, newOwnerUserId)) return FALSE; // 새 팀장 후보가 팀원인지

    if (!UpdateTeamsOwner(teamId, newOwnerUserId)) return FALSE;
    if (!RewriteMembersForTransfer(teamId, requestUserId, newOwnerUserId)) return FALSE;

    return TRUE;
}

// ------------------------------------------------------------
// ✅ 팀 탈퇴(자동 위임)
// - OWNER 탈퇴면: team_members.txt에서 "파일상 먼저 나온 순서"로 새 OWNER 선정
// - 새 OWNER 있으면: teams.txt ownerId 변경 + 멤버 role도 OWNER로 변경
// - 새 OWNER 없으면: 팀 삭제
// ------------------------------------------------------------
int Team_LeaveAutoTransfer(const wchar_t* teamId, const wchar_t* userId)
{
    if (!teamId || !teamId[0] || !userId || !userId[0]) return 0;

    TeamInfo t;
    if (!Team_FindByTeamId(teamId, &t)) return 0;

    const int isOwner = (wcscmp(t.ownerUserId, userId) == 0);

    // 1) OWNER면 "다음 멤버(파일 순서)" 찾기
    wchar_t newOwner[128] = { 0 };
    if (isOwner)
    {
        FILE* fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
        if (!fp) fp = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");

        if (fp)
        {
            wchar_t line[512];
            while (fgetws(line, 512, fp))
            {
                wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
                TrimNewline(line);
                if (swscanf(line, L"%31[^|]|%127[^|]|%31[^\n]", tid, uid, role) == 3)
                {
                    if (wcscmp(tid, teamId) != 0) continue;
                    if (wcscmp(uid, userId) == 0) continue; // 본인은 제외
                    lstrcpynW(newOwner, uid, 128);
                    break;
                }
            }
            fclose(fp);
        }

        // 새 OWNER가 없으면 팀 삭제
        if (!newOwner[0]) {
            return DeleteTeamHard(teamId);
        }

        if (!UpdateTeamsOwner(teamId, newOwner)) return 0;
    }

    // 2) team_members.txt 다시 써서
    //    - 탈퇴자 제거
    //    - (OWNER였던 경우) newOwner는 OWNER로
    {
        FILE* in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r, ccs=UTF-8");
        if (!in) in = wfopen_utf8(TEAM_MEMBERS_FILE, L"r");
        if (!in) return 1;

        FILE* out = wfopen_utf8(L"team_members_tmp.txt", L"w, ccs=UTF-8");
        if (!out) out = wfopen_utf8(L"team_members_tmp.txt", L"w");
        if (!out) { fclose(in); return 1; }

        wchar_t line[512];
        while (fgetws(line, 512, in))
        {
            wchar_t tid[32] = { 0 }, uid[128] = { 0 }, role[32] = { 0 };
            TrimNewline(line);

            if (swscanf(line, L"%31[^|]|%127[^|]|%31[^\n]", tid, uid, role) == 3)
            {
                if (wcscmp(tid, teamId) != 0) {
                    fwprintf(out, L"%s|%s|%s\n", tid, uid, role);
                    continue;
                }

                if (wcscmp(uid, userId) == 0) continue; // 탈퇴자 제거

                if (isOwner && newOwner[0] && wcscmp(uid, newOwner) == 0) {
                    fwprintf(out, L"%s|%s|%s\n", tid, uid, L"OWNER");
                    continue;
                }

                fwprintf(out, L"%s|%s|%s\n", tid, uid, role);
                continue;
            }
        }

        fclose(in);
        fclose(out);
        _wremove(TEAM_MEMBERS_FILE);
        _wrename(L"team_members_tmp.txt", TEAM_MEMBERS_FILE);
    }

    return 1;
}