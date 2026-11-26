#pragma once
#include <windows.h>
#include <string>
namespace crc {
struct Palette {
    COLORREF background, surface, field, text, muted, border, accent, accentText, hover, selection, danger;
    static Palette system(bool dark);
};
void roundedFill(HDC dc,RECT rect,int radius,COLORREF fill,COLORREF border);
void styleTitleBar(HWND window,bool dark);
HICON makeTrayIcon();
}
