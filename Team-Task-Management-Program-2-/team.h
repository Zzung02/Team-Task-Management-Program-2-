// team.h
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

    typedef struct {
        wchar_t teamId[32];
        wchar_t teamName[128];
        wchar_t joinCode[64];
        wchar_t ownerUserId[128];
    } TeamInfo;

    typedef struct {
        wchar_t teamId[32];
        wchar_t userId[128];
        wchar_t role[32];   // OWNER / MEMBER
    } TeamMember;


    // ====== 기본 기능 ======
    BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam);
    BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam);
    BOOL Team_IsOwner(const wchar_t* teamId, const wchar_t* userId);
    int  Team_LoadMembers(const wchar_t* teamId, TeamMember* outArr, int maxCount);


    // ====== 삭제 / 위임 ======
    BOOL Team_DeleteTeam(const wchar_t* teamId, const wchar_t* requestUserId);
    BOOL Team_TransferOwner(const wchar_t* teamId,
        const wchar_t* requestUserId,
        const wchar_t* newOwnerUserId);
    // ====== 팀 생성/참여 (app.c에서 사용) ======
    BOOL Team_Create(const wchar_t* teamName, const wchar_t* joinCode,
        const wchar_t* ownerUserId, TeamInfo* outTeam);

    BOOL Team_JoinByCode(const wchar_t* joinCode, const wchar_t* userId,
        TeamInfo* outTeam);

    const wchar_t* RoleToKorean(const wchar_t* role);
#ifdef __cplusplus
}
#endif