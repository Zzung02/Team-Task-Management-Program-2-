#include "bmp_loader.h"
#include <wchar.h>

static void GetExeDir(wchar_t* outDir, int outCap)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    wcsncpy_s(outDir, outCap, path, _TRUNCATE);
}

static void BuildPathExeDir(wchar_t* out, int outCap, const wchar_t* file)
{
    wchar_t dir[MAX_PATH];
    GetExeDir(dir, MAX_PATH);
    wcsncpy_s(out, outCap, dir, _TRUNCATE);
    wcscat_s(out, outCap, file);
}

HBITMAP LoadBmpFromExeDir(HWND hWnd, const wchar_t* filename, int* outW, int* outH)
{
    wchar_t full[MAX_PATH];
    BuildPathExeDir(full, MAX_PATH, filename);

    HBITMAP hb = (HBITMAP)LoadImageW(NULL, full, IMAGE_BITMAP, 0, 0,
        LR_LOADFROMFILE | LR_CREATEDIBSECTION);

    if (!hb) {
        wchar_t msg[512];
        wsprintfW(msg,
            L"BMP 로드 실패: %s\n\n"
            L"exe 폴더 기준 경로:\n%s",
            filename, full);
        MessageBoxW(hWnd, msg, L"Error", MB_ICONERROR);
        return NULL;
    }

    BITMAP bm;
    GetObject(hb, sizeof(bm), &bm);
    if (outW) *outW = bm.bmWidth;
    if (outH) *outH = bm.bmHeight;
    return hb;
}

void SetWindowClientSize(HWND hWnd, int clientW, int clientH)
{
    RECT rc = { 0,0, clientW, clientH };
    DWORD style = (DWORD)GetWindowLongPtrW(hWnd, GWL_STYLE);
    DWORD ex = (DWORD)GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    AdjustWindowRectEx(&rc, style, FALSE, ex);
    SetWindowPos(hWnd, NULL, 0, 0,
        rc.right - rc.left, rc.bottom - rc.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void DrawBitmapFit(HDC dst, HBITMAP bmp, int dstW, int dstH)
{
    if (!bmp) return;

    BITMAP bm;
    GetObject(bmp, sizeof(bm), &bm);

    HDC bmpDC = CreateCompatibleDC(dst);
    HBITMAP old = (HBITMAP)SelectObject(bmpDC, bmp);

    SetStretchBltMode(dst, HALFTONE);
    StretchBlt(dst, 0, 0, dstW, dstH,
        bmpDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

    SelectObject(bmpDC, old);
    DeleteDC(bmpDC);
}
