#include "paint.hpp"
#include "theme.hpp"
#include "platform/platform.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
namespace crc {
static void rounded(Gdiplus::GraphicsPath& p,float x,float y,float w,float h,float r) {
    float d=r*2; p.AddArc(x,y,d,d,180,90); p.AddArc(x+w-d,y,d,d,270,90);
    p.AddArc(x+w-d,y+h-d,d,d,0,90); p.AddArc(x,y+h-d,d,d,90,90); p.CloseFigure();
}
void paintCard(HWND window,UINT dpi,bool result,const std::wstring& title,const std::wstring& subtitle,BYTE opacity,bool dark) {
    using namespace Gdiplus;
    RECT wr{}; GetWindowRect(window,&wr); int w=wr.right-wr.left,h=wr.bottom-wr.top;
    if(w<1||h<1) return;
    Bitmap image(w,h,PixelFormat32bppPARGB);
    {
        Graphics g(&image); g.Clear(Color(0,0,0,0)); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        float pad=static_cast<float>(px(6,dpi));
        for(int i=5;i>=1;--i) { GraphicsPath shadow; float s=static_cast<float>(px(i,dpi)); rounded(shadow,pad-s,pad-s+px(2,dpi),w-2*pad+2*s,h-2*pad+2*s,static_cast<float>(px(result?22:18,dpi))); SolidBrush brush(Color(static_cast<BYTE>(5),0,0,0)); g.FillPath(&brush,&shadow); }
        GraphicsPath card; rounded(card,pad,pad,w-2*pad,h-2*pad,static_cast<float>(px(result?18:16,dpi)));
        auto palette=Palette::system(dark); auto color=[](COLORREF c){return Color(255,GetRValue(c),GetGValue(c),GetBValue(c));};
        SolidBrush background(color(palette.surface)); g.FillPath(&background,&card);
        Pen edge(color(palette.border),static_cast<float>(px(1,dpi))); g.DrawPath(&edge,&card);
        if(!result) { SolidBrush accent(color(palette.accent)); g.FillEllipse(&accent,static_cast<float>(px(17,dpi)),static_cast<float>(h)/2-px(3,dpi),static_cast<float>(px(6,dpi)),static_cast<float>(px(6,dpi))); }
        if(result) { SolidBrush accent(dark?Color(255,137,178,255):Color(255,44,99,211)); g.FillEllipse(&accent,static_cast<float>(w)/2-px(3,dpi),static_cast<float>(px(24,dpi)),static_cast<float>(px(6,dpi)),static_cast<float>(px(6,dpi))); }
        StringFormat format; format.SetAlignment(StringAlignmentCenter); format.SetLineAlignment(StringAlignmentCenter); format.SetTrimming(StringTrimmingEllipsisCharacter);
        SolidBrush text(color(palette.text));
        FontFamily family(L"Segoe UI");
        float fontSize=static_cast<float>(px(result?48:20,dpi));
        RectF area(pad+px(12,dpi),pad+(result?px(20,dpi):0),w-2*pad-px(24,dpi),h-2*pad-(result?px(55,dpi):0));
        for(;;) { Font font(&family,fontSize,FontStyleBold,UnitPixel); RectF measure; g.MeasureString(title.c_str(),-1,&font,PointF(),&measure); if(measure.Width<=area.Width||fontSize<=px(result?18:10,dpi)) { g.DrawString(title.c_str(),-1,&font,area,&format,&text); break; } fontSize-=2; }
        if(result) { Font captionFont(&family,static_cast<float>(px(13,dpi)),FontStyleRegular,UnitPixel); SolidBrush muted(dark?Color(255,170,182,200):Color(255,100,112,130)); RectF sub(pad,static_cast<float>(h-px(52,dpi)),w-2*pad,static_cast<float>(px(25,dpi))); g.DrawString(subtitle.c_str(),-1,&captionFont,sub,&format,&muted); }
    }
    HBITMAP bitmap=nullptr; image.GetHBITMAP(Gdiplus::Color(0,0,0,0),&bitmap);
    HDC screen=GetDC(nullptr), memory=CreateCompatibleDC(screen); HGDIOBJ old=SelectObject(memory,bitmap);
    POINT pos{wr.left,wr.top},origin{}; SIZE size{w,h}; BLENDFUNCTION blend{AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
    UpdateLayeredWindow(window,screen,&pos,&size,memory,&origin,0,&blend,ULW_ALPHA);
    SelectObject(memory,old); DeleteDC(memory); ReleaseDC(nullptr,screen); DeleteObject(bitmap);
}
}
