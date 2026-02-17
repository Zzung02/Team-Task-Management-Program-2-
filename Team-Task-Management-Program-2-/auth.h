//auth.h


#pragma once
#include <windows.h>

BOOL Auth_Login(const wchar_t* id, const wchar_t* pw);
BOOL Auth_Signup(const wchar_t* id, const wchar_t* pw, const wchar_t* name);
BOOL Auth_FindPassword(const wchar_t* id, const wchar_t* name, wchar_t* outPw, int outPwLen);
BOOL Auth_GetUserNameById(const wchar_t* id, wchar_t* outName, int outNameLen);
int Auth_GetUserNameById(const wchar_t* userId, wchar_t* outName, int outLen);
