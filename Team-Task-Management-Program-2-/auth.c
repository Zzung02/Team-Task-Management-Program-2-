#define _CRT_SECURE_NO_WARNINGS
#include "auth.h"
#include <stdio.h>
#include <wchar.h>

static const wchar_t* USERS_FILE = L"users.txt";

// 줄 끝 개행 제거
static void TrimNewline(wchar_t* s)
{
    if (!s) return;
    size_t n = wcslen(s);
    while (n > 0 && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) {
        s[n - 1] = 0;
        n--;
    }
}

// "id|pw|name" 파싱
static int ParseUserLine(const wchar_t* line,
    wchar_t* outId, int idLen,
    wchar_t* outPw, int pwLen,
    wchar_t* outName, int nameLen)
{
    if (!line || !outId || !outPw || !outName) return 0;

    wchar_t buf[512];
    wcsncpy(buf, line, 511);
    buf[511] = 0;
    TrimNewline(buf);

    wchar_t* p1 = wcschr(buf, L'|');
    if (!p1) return 0;
    *p1++ = 0;

    wchar_t* p2 = wcschr(p1, L'|');
    if (!p2) return 0;
    *p2++ = 0;

    wcsncpy(outId, buf, idLen - 1);    outId[idLen - 1] = 0;
    wcsncpy(outPw, p1, pwLen - 1);     outPw[pwLen - 1] = 0;
    wcsncpy(outName, p2, nameLen - 1); outName[nameLen - 1] = 0;
    return 1;
}

// 파일 열기(UTF-8 권장)
static FILE* OpenUsersFileRead(void)
{
    FILE* fp = _wfopen(USERS_FILE, L"r, ccs=UTF-8");
    if (!fp) fp = _wfopen(USERS_FILE, L"r");
    return fp;
}

static FILE* OpenUsersFileAppend(void)
{
    FILE* fp = _wfopen(USERS_FILE, L"a, ccs=UTF-8");
    if (!fp) fp = _wfopen(USERS_FILE, L"a");
    return fp;
}

BOOL Auth_Login(const wchar_t* id, const wchar_t* pw)
{
    if (!id || !pw || !id[0] || !pw[0]) return FALSE;

    FILE* fp = OpenUsersFileRead();
    if (!fp) return FALSE;

    wchar_t line[512];
    wchar_t fid[128], fpw[128], fname[128];

    while (fgetws(line, 512, fp))
    {
        if (!ParseUserLine(line, fid, 128, fpw, 128, fname, 128))
            continue;

        if (wcscmp(fid, id) == 0 && wcscmp(fpw, pw) == 0) {
            fclose(fp);
            return TRUE;
        }
    }

    fclose(fp);
    return FALSE;
}

BOOL Auth_Signup(const wchar_t* id, const wchar_t* pw, const wchar_t* name)
{
    if (!id || !pw || !name || !id[0] || !pw[0] || !name[0]) return FALSE;

    // 1) 중복 아이디 체크
    FILE* fp = OpenUsersFileRead();
    if (fp) {
        wchar_t line[512];
        wchar_t fid[128], fpw[128], fname[128];

        while (fgetws(line, 512, fp))
        {
            if (!ParseUserLine(line, fid, 128, fpw, 128, fname, 128))
                continue;

            if (wcscmp(fid, id) == 0) {
                fclose(fp);
                return FALSE; // 이미 존재
            }
        }
        fclose(fp);
    }

    // 2) 저장(append)
    FILE* fa = OpenUsersFileAppend();
    if (!fa) return FALSE;

    // 한 줄 형식: id|pw|name
    fwprintf(fa, L"%ls|%ls|%ls\n", id, pw, name);
    fclose(fa);
    return TRUE;
}

BOOL Auth_FindPassword(const wchar_t* id, const wchar_t* name, wchar_t* outPw, int outPwLen)
{
    if (!id || !name || !outPw || outPwLen <= 0) return FALSE;
    outPw[0] = 0;

    FILE* fp = OpenUsersFileRead();
    if (!fp) return FALSE;

    wchar_t line[512];
    wchar_t fid[128], fpw[128], fname[128];

    while (fgetws(line, 512, fp))
    {
        if (!ParseUserLine(line, fid, 128, fpw, 128, fname, 128))
            continue;

        if (wcscmp(fid, id) == 0 && wcscmp(fname, name) == 0)
        {
            wcsncpy(outPw, fpw, outPwLen - 1);
            outPw[outPwLen - 1] = 0;
            fclose(fp);
            return TRUE;
        }
    }

    fclose(fp);
    return FALSE;
}
