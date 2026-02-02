#define _CRT_SECURE_NO_WARNINGS
#include "auth.h"
#include <stdio.h>
#include <wchar.h>

static void BuildUserFilePath(wchar_t* outPath, int cch)
{
    GetModuleFileNameW(NULL, outPath, cch);
    wchar_t* p = wcsrchr(outPath, L'\\');
    if (p) *(p + 1) = 0;
    wcscat_s(outPath, cch, L"users.txt");
}

static void TrimNewlineW(wchar_t* s)
{
    if (!s) return;
    size_t n = wcslen(s);
    while (n && (s[n - 1] == L'\n' || s[n - 1] == L'\r')) s[--n] = 0;
}

static int ParseLine(const wchar_t* line,
    wchar_t* id, int idCch,
    wchar_t* pw, int pwCch,
    wchar_t* name, int nameCch)
{
    // format: id|pw|name
    if (!line) return 0;
    const wchar_t* p1 = wcschr(line, L'|');
    if (!p1) return 0;
    const wchar_t* p2 = wcschr(p1 + 1, L'|');
    if (!p2) return 0;

    wcsncpy_s(id, idCch, line, p1 - line);
    wcsncpy_s(pw, pwCch, p1 + 1, p2 - (p1 + 1));
    wcsncpy_s(name, nameCch, p2 + 1, _TRUNCATE);
    TrimNewlineW(name);
    return 1;
}

static int ExistsId(const wchar_t* targetId)
{
    wchar_t path[MAX_PATH];
    BuildUserFilePath(path, MAX_PATH);

    FILE* fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512], id[128], pw[128], name[128];
    while (fgetws(line, 512, fp)) {
        if (!ParseLine(line, id, 128, pw, 128, name, 128)) continue;
        if (wcscmp(id, targetId) == 0) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

int Auth_Signup(const wchar_t* id, const wchar_t* pw, const wchar_t* name)
{
    if (!id || !pw || !name) return 0;
    if (id[0] == 0 || pw[0] == 0 || name[0] == 0) return 0;

    if (ExistsId(id)) return -1;

    wchar_t path[MAX_PATH];
    BuildUserFilePath(path, MAX_PATH);

    FILE* fp = _wfopen(path, L"a, ccs=UTF-8");
    if (!fp) return 0;

    fwprintf(fp, L"%s|%s|%s\n", id, pw, name);
    fclose(fp);
    return 1;
}

int Auth_Login(const wchar_t* id, const wchar_t* pw)
{
    wchar_t path[MAX_PATH];
    BuildUserFilePath(path, MAX_PATH);

    FILE* fp = _wfopen(path, L"r, ccs=UTF-8");
    if (!fp) return 0;

    wchar_t line[512], fid[128], fpw[128], fname[128];
    while (fgetws(line, 512, fp)) {
        if (!ParseLine(line, fid, 128, fpw, 128, fname, 128)) continue;
        if (wcscmp(fid, id) == 0 && wcscmp(fpw, pw) == 0) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}
