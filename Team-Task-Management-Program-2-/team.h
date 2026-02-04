//team.h

#pragma once
#include <windows.h>

#ifndef TEAMS_FILE
#define TEAMS_FILE          L"teams.txt"
#endif

#ifndef TEAM_MEMBERS_FILE
#define TEAM_MEMBERS_FILE   L"team_members.txt"
#endif

typedef struct TeamInfo {
    wchar_t teamId[32];
    wchar_t teamName[128];
    wchar_t taskName[128];
    wchar_t joinCode[64];
    wchar_t ownerUserId[128];
} TeamInfo;

// ✅ 팀 생성(teams.txt + team_members.txt 저장)
// - joinCode 중복이면 실패
BOOL Team_Create(const wchar_t* teamName,
    const wchar_t* taskName,
    const wchar_t* joinCode,
    const wchar_t* ownerUserId,
    TeamInfo* outTeam); // outTeam NULL 가능

// ✅ 팀 참여(teams.txt에서 joinCode로 teamId 찾고,
//            team_members.txt에 멤버 추가)
// - 이미 가입한 멤버면 실패(중복 방지)
BOOL Team_JoinByCode(const wchar_t* joinCode,
    const wchar_t* userId,
    TeamInfo* outTeam); // outTeam NULL 가능

// ✅ joinCode로 팀 정보 찾기
BOOL Team_FindByJoinCode(const wchar_t* joinCode, TeamInfo* outTeam);

// ✅ teamId로 팀 정보 찾기
BOOL Team_FindByTeamId(const wchar_t* teamId, TeamInfo* outTeam);
