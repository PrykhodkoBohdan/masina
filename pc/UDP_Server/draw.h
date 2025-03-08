#pragma once
#include "common.h"
#include "utils.h"

void ResizeRenderTarget(HWND hwnd);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void WindowUpdate(HWND hwnd, HWND targetWnd, const char* windowName);
void DrawOutlineText(const wchar_t* text, D2D1_RECT_F rect);
void DrawOverlay(HWND hwnd, const char* telemetryStr);
void InitD2D(HWND hwnd);
void CleanupD2D();