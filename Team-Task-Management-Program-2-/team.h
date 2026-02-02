#pragma once
#include <wchar.h>

// teams.txt : team|task|code
int Team_Create(const wchar_t* team, const wchar_t* task, const wchar_t* code);

// team_members.txt : userId|joinCode
int Team_AddMember(const wchar_t* userId, const wchar_t* code);

// 내가 속한 팀 코드들
int Team_GetMyTeams(const wchar_t* userId, wchar_t outCodes[][128], int max);

// 참여코드로 팀/과제 찾기 (teams.txt에서 찾음)
int Team_FindByCode(const wchar_t* joinCode,
    wchar_t* outTeam, int outTeamCap,
    wchar_t* outTask, int outTaskCap);
