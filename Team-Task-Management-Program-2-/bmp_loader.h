#pragma once
#include <windows.h>

HBITMAP LoadBmpFromExeDir(HWND hWnd, const wchar_t* filename, int* outW, int* outH);
void    DrawBitmapFit(HDC dst, HBITMAP bmp, int dstW, int dstH);
void    SetWindowClientSize(HWND hWnd, int clientW, int clientH);
