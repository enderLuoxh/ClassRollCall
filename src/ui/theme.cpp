#include "theme.hpp"
#include <objidl.h>
#include <gdiplus.h>
namespace crc {
Palette Palette::system(bool dark) {
    if(dark) return {RGB(25,27,32),RGB(33,36,43),RGB(41,44,52),RGB(240,242,248),RGB(161,169,186),RGB(61,66,78),RGB(137,178,255),RGB(18,35,65),RGB(49,55,67),RGB(47,64,91),RGB(255,157,157)};
    return {RGB(245,247,251),RGB(255,255,255),RGB(255,255,255),RGB(28,38,56),RGB(107,119,140),RGB(222,228,237),RGB(44,99,211),RGB(255,255,255),RGB(238,243,251),RGB(231,239,255),RGB(187,55,65)};
}
void roundedFill(HDC dc,RECT r,int radius,COLORREF fill,COLORREF border) {
    using namespace Gdiplus;
    if(r.right<=r.left||r.bottom<=r.top) return;
    Graphics g(dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
    float x=static_cast<float>(r.left)+0.5f,y=static_cast<float>(r.top)+0.5f,w=static_cast<float>(r.right-r.left)-1,h=static_cast<float>(r.bottom-r.top)-1;
    float d=static_cast<float>(radius*2); if(d>w) d=w; if(d>h) d=h;
    GraphicsPath p; p.AddArc(x,y,d,d,180,90); p.AddArc(x+w-d,y,d,d,270,90); p.AddArc(x+w-d,y+h-d,d,d,0,90); p.AddArc(x,y+h-d,d,d,90,90); p.CloseFigure();
    SolidBrush brush(Color(255,GetRValue(fill),GetGValue(fill),GetBValue(fill))); g.FillPath(&brush,&p);
    Pen pen(Color(255,GetRValue(border),GetGValue(border),GetBValue(border)),1.f); g.DrawPath(&pen,&p);
}
void styleTitleBar(HWND h,bool dark) {
    static HMODULE dwm=LoadLibraryW(L"dwmapi.dll");
    using Set=HRESULT(WINAPI*)(HWND,DWORD,LPCVOID,DWORD);
    static auto set=dwm?reinterpret_cast<Set>(GetProcAddress(dwm,"DwmSetWindowAttribute")):nullptr;
    if(set) { BOOL value=dark; if(FAILED(set(h,20,&value,sizeof value))) set(h,19,&value,sizeof value); DWORD corner=2; set(h,33,&corner,sizeof corner); }
}
HICON makeTrayIcon() {
    using namespace Gdiplus;
    Bitmap b(32,32,PixelFormat32bppPARGB); Graphics g(&b); g.Clear(Color(0,0,0,0)); g.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush blue(Color(255,44,99,211)); g.FillEllipse(&blue,1,1,30,30);
    Pen white(Color(255,255,255,255),2.4f); g.DrawLine(&white,9,16,14,21); g.DrawLine(&white,14,21,23,11);
    HICON icon=nullptr; b.GetHICON(&icon); return icon;
}
}
