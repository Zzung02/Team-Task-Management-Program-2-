// team.h  (원샷 교체용)
#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef TEAMS_FILE
#define TEAMS_FILE          L"teams.txt"
#endif

#ifndef TEAM_MEMBERS_FILE
#define TEAM_MEMBERS_FILE   L"team_members.txt"
#endif

    // -------------------------
    // TeamInfo
    // -------------------------
    typedef struct TeamInfo {
        wchar_t teamId[32];
        wchar_t teamName[128];
        wchar_t joinCode[64];
        wchar_t ownerUserId[128];   // ✅ 팀 생성한 최초 사용자(팀장)
    } TeamInfo;

    // -------------------------
    // TeamMember  (✅ 누락돼서 추가 필요)
    // team_members.txt line: teamId|userId|role
    // -------------------------
    typedef struct TeamMember {
        wchar_t teamId[32];
        wchar_t userId[128];
        wchar_t role[32];           // 예: "OWNER", "MEMBER", "LEADER", ...
    } TeamMember;

    // -------------------------
    // 팀 생성/참여/조회
    // -------------------------
    BOOL Team_Create(const wchar_t* teamName,
        const wchar_t* joinCode,
        const wchar_t* ownerUserId,
        TeamInfo* outTeam); // outTeam NULL 가능

    BOOL Team_JoinByCode(const wchar_t* joinCode,
        const wchar_t* userId,
        TeamInfo* outTeam); // outTeam NULL 가능

    BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam);
    BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam);

    // -------------------------
    // 팀원 관리
    // -------------------------
    int  Team_LoadMembers(const wchar_t* teamId, TeamMember* outArr, int maxCount);

    // owner인지(팀장인지) 체크: teams.txt의 ownerUserId와 비교
    BOOL Team_IsOwner(const wchar_t* teamId, const wchar_t* userId);

    // 팀원 역할 변경 (✅ 팀장만 App에서 호출하도록)
    BOOL Team_SetMemberRole(const wchar_t* teamId,
        const wchar_t* memberUserId,
        const wchar_t* newRole);

#ifdef __cplusplus
}
#endif