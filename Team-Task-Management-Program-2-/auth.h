#pragma once
#include <windows.h>

int Auth_Signup(const wchar_t* id, const wchar_t* pw, const wchar_t* name);
// return: 1 성공, -1 중복ID, 0 실패

int Auth_Login(const wchar_t* id, const wchar_t* pw);
// return: 1 성공, 0 실패
