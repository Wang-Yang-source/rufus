/* Optional ProjectGraph visual adapter for the existing Win32 dialog. */
#pragma once
#include <windows.h>

void InitProjectGraphUI(HWND window);

void PaintProjectGraphFrame(HWND window, HDC dc);
