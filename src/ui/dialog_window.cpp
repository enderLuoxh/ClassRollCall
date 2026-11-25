#include "dialog_window.hpp"
#include "dialogs.hpp"
#include "platform/platform.hpp"
#include <windowsx.h>
#include <uxtheme.h>
#include <algorithm>
#include <stdexcept>
namespace crc {
static std::wstring caption(HWND h) { int n=GetWindowTextLengthW(h); std::wstring value(n+1,0); GetWindowTextW(h,value.data(),n+1); value.resize(n); return value; }
DialogWindow::Control* DialogWindow::control(HWND h) { for(auto& c:controls_) if(c.handle==h) return &c; return nullptr; }
COLORREF DialogWindow::backing(const Control& c) const { if(c.y>=height-80) return palette_.surface; POINT p{c.x+c.w/2,c.y+c.h/2}; for(auto r:cards_) if(PtInRect(&r,p)) return palette_.surface; return palette_.background; }
void DialogWindow::refreshTheme() { dark_=darkTheme(); palette_=Palette::system(dark_); styleTitleBar(hwnd,dark_); InvalidateRect(hwnd,nullptr,TRUE); for(auto& c:controls_) InvalidateRect(c.handle,nullptr,TRUE); }
void DialogWindow::event(int id,int) { if(id==IDCANCEL) finish(false); }
void DialogWindow::finish(bool yes) { accepted_=yes; done_=true; DestroyWindow(hwnd); }
void DialogWindow::header(const std::wstring& title,const std::wstring& subtitle) { label(title,28,22,width-56,36,Role::Title); label(subtitle,28,64,width-56,24,Role::Muted); }
void DialogWindow::card(int x,int y,int w,int h) { cards_.push_back({x,y,x+w,y+h}); }
HWND DialogWindow::item(int id) { return GetDlgItem(hwnd,id); }
HWND DialogWindow::add(const wchar_t* cls,const std::wstring& title,DWORD style,int id,int x,int y,int w,int h,Role role) {
    bool multiline=false;
    if(wcscmp(cls,L"EDIT")==0) { role=Role::Input; multiline=(style&ES_MULTILINE)!=0; }
    else if(wcscmp(cls,L"COMBOBOX")==0) { role=Role::Combo; style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS; }
    else if(wcscmp(cls,L"LISTBOX")==0) { role=Role::List; style|=LBS_OWNERDRAWFIXED|LBS_HASSTRINGS; }
    else if(wcscmp(cls,L"BUTTON")==0) { if((style&BS_TYPEMASK)==BS_AUTOCHECKBOX) role=Role::Toggle; else style=(style&~BS_TYPEMASK)|BS_OWNERDRAW; }
    int insetX=role==Role::Input?12:0,insetY=role==Role::Input?(multiline?8:12):0;
    HWND child=CreateWindowExW(0,cls,title.c_str(),WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|style,px(x+insetX,dpi),px(y+insetY,dpi),px(w-2*insetX,dpi),px(h-2*insetY+(role==Role::Combo?384:0),dpi),hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    if(!child) throw std::runtime_error("无法创建界面控件");
    controls_.push_back({child,x,y,w,h,role,multiline,false});
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(role==Role::Title?titleFont_:role==Role::Muted?smallFont_:font_),TRUE);
    if(role==Role::Combo) { SendMessageW(child,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(h-6,dpi)); SendMessageW(child,CB_SETITEMHEIGHT,0,px(48,dpi)); }
    if(role==Role::List) SendMessageW(child,LB_SETITEMHEIGHT,0,px(48,dpi));
    if(role==Role::Input||role==Role::List) SetWindowTheme(child,L"Explorer",nullptr);
    SetWindowSubclass(child,childProc,1,reinterpret_cast<DWORD_PTR>(this));
    return child;
}
HWND DialogWindow::button(const std::wstring& title,int id,int x,int y,int w,Role role) { if(id==IDOK&&role==Role::Secondary) role=Role::Primary; return add(L"BUTTON",title,WS_TABSTOP|BS_PUSHBUTTON,id,x,y,w,48,role); }
void DialogWindow::label(const std::wstring& title,int x,int y,int w,int h,Role role) { add(L"STATIC",title,SS_LEFT|SS_NOPREFIX,0,x,y,w,h,role); }
void DialogWindow::paint(HDC dc) {
    RECT client{}; GetClientRect(hwnd,&client); HBRUSH bg=CreateSolidBrush(palette_.background); FillRect(dc,&client,bg); DeleteObject(bg);
    for(auto r:cards_) { RECT box{px(r.left,dpi),px(r.top,dpi),px(r.right,dpi),px(r.bottom,dpi)}; roundedFill(dc,box,px(14,dpi),palette_.surface,palette_.border); }
    RECT footer{0,px(height-80,dpi),client.right,client.bottom}; HBRUSH surface=CreateSolidBrush(palette_.surface); FillRect(dc,&footer,surface); DeleteObject(surface);
    HPEN line=CreatePen(PS_SOLID,1,palette_.border); auto oldPen=SelectObject(dc,line); MoveToEx(dc,0,footer.top,nullptr); LineTo(dc,client.right,footer.top); SelectObject(dc,oldPen); DeleteObject(line);
    for(auto& c:controls_) if(c.role==Role::Input) { RECT box{px(c.x,dpi),px(c.y,dpi),px(c.x+c.w,dpi),px(c.y+c.h,dpi)}; roundedFill(dc,box,px(8,dpi),palette_.field,GetFocus()==c.handle?palette_.accent:palette_.border); }
}
void DialogWindow::drawControl(DRAWITEMSTRUCT* d) {
    auto* c=control(d->hwndItem); if(!c) return;
    HDC dc=d->hDC; RECT r=d->rcItem; auto oldFont=SelectObject(dc,font_); SetBkMode(dc,TRANSPARENT);
    if(d->CtlType==ODT_LISTBOX||d->CtlType==ODT_COMBOBOX) {
        SetDCBrushColor(dc,palette_.surface); FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        if(d->itemID!=static_cast<UINT>(-1)) {
            bool list=d->CtlType==ODT_LISTBOX; bool selected=(d->itemState&ODS_SELECTED)!=0;
            RECT box=r; InflateRect(&box,-px(4,dpi),-px(3,dpi)); if(selected) roundedFill(dc,box,px(7,dpi),palette_.selection,palette_.selection);
            LRESULT n=SendMessageW(c->handle,list?LB_GETTEXTLEN:CB_GETLBTEXTLEN,d->itemID,0);
            if(n>=0&&n<32768) { std::wstring value(static_cast<size_t>(n)+1,0); SendMessageW(c->handle,list?LB_GETTEXT:CB_GETLBTEXT,d->itemID,reinterpret_cast<LPARAM>(value.data())); value.resize(static_cast<size_t>(n));
                auto split=value.find(L'\t'); RECT area=r; area.left+=px(16,dpi); area.right-=px(16,dpi); SetTextColor(dc,palette_.text);
                if(split!=value.npos) { auto count=value.substr(split+1); value.resize(split); RECT badge=area; badge.left=std::max(badge.left,badge.right-px(90,dpi)); SetTextColor(dc,palette_.muted); DrawTextW(dc,count.c_str(),-1,&badge,DT_SINGLELINE|DT_VCENTER|DT_RIGHT|DT_NOPREFIX); area.right=badge.left-px(12,dpi); }
                SetTextColor(dc,palette_.text); DrawTextW(dc,value.c_str(),-1,&area,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
            }
        }
    } else {
        bool primary=c->role==Role::Primary,disabled=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0;
        COLORREF fill=primary?palette_.accent:(c->hover||pressed?palette_.hover:palette_.surface);
        if(disabled) fill=palette_.hover;
        SetDCBrushColor(dc,backing(*c)); FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        RECT box=r; InflateRect(&box,-px(1,dpi),-px(1,dpi)); roundedFill(dc,box,px(9,dpi),fill,primary&&!disabled?fill:palette_.border);
        SetTextColor(dc,disabled?palette_.muted:primary?palette_.accentText:c->role==Role::Danger?palette_.danger:palette_.text);
        auto value=caption(c->handle); DrawTextW(dc,value.c_str(),-1,&box,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        if((d->itemState&ODS_FOCUS)&&!(d->itemState&ODS_NOFOCUSRECT)) { InflateRect(&box,-px(4,dpi),-px(4,dpi)); DrawFocusRect(dc,&box); }
    }
    SelectObject(dc,oldFont);
}
void DialogWindow::drawToggle(HWND h,HDC dc) {
    auto* c=control(h); if(!c) return; RECT r{}; GetClientRect(h,&r); SetDCBrushColor(dc,backing(*c)); FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    bool checked=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED; int top=(r.bottom-px(24,dpi))/2;
    RECT track{0,top,px(42,dpi),top+px(24,dpi)}; roundedFill(dc,track,px(12,dpi),checked?palette_.accent:palette_.hover,checked?palette_.accent:palette_.border);
    int knobX=checked?px(21,dpi):px(3,dpi); RECT knob{knobX,top+px(3,dpi),knobX+px(18,dpi),top+px(21,dpi)};
    roundedFill(dc,knob,px(9,dpi),checked?palette_.accentText:palette_.muted,checked?palette_.accentText:palette_.muted);
    auto old=SelectObject(dc,font_); SetTextColor(dc,palette_.text); SetBkMode(dc,TRANSPARENT); r.left=px(54,dpi); auto title=caption(h); DrawTextW(dc,title.c_str(),-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX); if(GetFocus()==h) { InflateRect(&r,-2,-2); DrawFocusRect(dc,&r); } SelectObject(dc,old);
}
void DialogWindow::drawClosedCombo(HWND h,HDC dc) {
    auto* c=control(h); if(!c) return; RECT r{}; GetClientRect(h,&r); SetDCBrushColor(dc,backing(*c)); FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    RECT box=r; InflateRect(&box,-1,-1); roundedFill(dc,box,px(8,dpi),c->hover?palette_.hover:palette_.field,GetFocus()==h?palette_.accent:palette_.border);
    auto old=SelectObject(dc,font_); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,palette_.text); auto value=caption(h); RECT area=r; area.left+=px(12,dpi); area.right-=px(36,dpi); DrawTextW(dc,value.c_str(),-1,&area,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX); SelectObject(dc,old);
    HPEN pen=CreatePen(PS_SOLID,px(2,dpi),palette_.muted); auto oldPen=SelectObject(dc,pen); int x=r.right-px(22,dpi),y=r.bottom/2-px(2,dpi); MoveToEx(dc,x-px(4,dpi),y,nullptr); LineTo(dc,x,y+px(4,dpi)); LineTo(dc,x+px(4,dpi),y); SelectObject(dc,oldPen); DeleteObject(pen);
}
LRESULT CALLBACK DialogWindow::childProc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
    auto* d=reinterpret_cast<DialogWindow*>(data); auto* c=d->control(h);
    if(msg==WM_NCDESTROY) { RemoveWindowSubclass(h,childProc,id); return DefSubclassProc(h,msg,wp,lp); }
    if(c) {
        if(msg==WM_MOUSEMOVE&&!c->hover) { c->hover=true; TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,h,0}; TrackMouseEvent(&track); InvalidateRect(h,nullptr,FALSE); }
        if(msg==WM_MOUSELEAVE) { c->hover=false; InvalidateRect(h,nullptr,FALSE); }
        if(msg==WM_PAINT&&c->role==Role::Toggle) { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); d->drawToggle(h,dc); EndPaint(h,&ps); return 0; }
        if(msg==WM_PAINT&&c->role==Role::Combo) { LRESULT result=DefSubclassProc(h,msg,wp,lp); HDC dc=GetDC(h); d->drawClosedCombo(h,dc); ReleaseDC(h,dc); return result; }
        if(msg==WM_SETFOCUS||msg==WM_KILLFOCUS) { InvalidateRect(d->hwnd,nullptr,FALSE); InvalidateRect(h,nullptr,FALSE); }
        if(msg==BM_SETCHECK||msg==WM_LBUTTONUP||msg==WM_KEYUP) { LRESULT result=DefSubclassProc(h,msg,wp,lp); InvalidateRect(h,nullptr,FALSE); return result; }
    }
    return DefSubclassProc(h,msg,wp,lp);
}
void DialogWindow::layout(const RECT* suggested) {
    for(HFONT* f:{&font_,&titleFont_,&smallFont_}) if(*f) { DeleteObject(*f); *f=nullptr; }
    auto make=[&](int size,int weight) { return CreateFontW(-px(size,dpi),0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI"); };
    font_=make(15,FW_NORMAL); titleFont_=make(26,FW_SEMIBOLD); smallFont_=make(13,FW_NORMAL);
    for(auto& c:controls_) { int ix=c.role==Role::Input?12:0,iy=c.role==Role::Input?(c.multiline?8:12):0;
        SetWindowPos(c.handle,nullptr,px(c.x+ix,dpi),px(c.y+iy,dpi),px(c.w-2*ix,dpi),px(c.h-2*iy+(c.role==Role::Combo?384:0),dpi),SWP_NOZORDER);
        SendMessageW(c.handle,WM_SETFONT,reinterpret_cast<WPARAM>(c.role==Role::Title?titleFont_:c.role==Role::Muted?smallFont_:font_),TRUE);
        if(c.role==Role::Combo) { SendMessageW(c.handle,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(c.h-6,dpi)); SendMessageW(c.handle,CB_SETITEMHEIGHT,0,px(48,dpi)); }
        if(c.role==Role::List) SendMessageW(c.handle,LB_SETITEMHEIGHT,0,px(48,dpi));
    }
    RECT outer{0,0,px(width,dpi),px(height,dpi)}; using Adjust=BOOL(WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT);
    static auto adjust=reinterpret_cast<Adjust>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"AdjustWindowRectExForDpi")); DWORD style=WS_POPUP|WS_CAPTION|WS_SYSMENU;
    if(adjust) adjust(&outer,style,FALSE,WS_EX_DLGMODALFRAME,dpi); else AdjustWindowRectEx(&outer,style,FALSE,WS_EX_DLGMODALFRAME);
    RECT current{}; GetWindowRect(hwnd,&current); int x=suggested?suggested->left:current.left,y=suggested?suggested->top:current.top;
    auto r=clampRect({x,y,x+outer.right-outer.left,y+outer.bottom-outer.top},workArea(MonitorFromPoint({x,y},MONITOR_DEFAULTTONEAREST)));
    SetWindowPos(hwnd,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE); InvalidateRect(hwnd,nullptr,TRUE);
}
LRESULT CALLBACK DialogWindow::proc(HWND h,UINT m,WPARAM w,LPARAM l) {
    auto* d=reinterpret_cast<DialogWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE) { d=static_cast<DialogWindow*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); d->hwnd=h; SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(d)); }
    if(d) try { switch(m) {
        case WM_COMMAND: d->event(LOWORD(w),HIWORD(w)); return 0;
        case WM_DPICHANGED: d->dpi=HIWORD(w); d->layout(reinterpret_cast<RECT*>(l)); return 0;
        case WM_SETTINGCHANGE: d->refreshTheme(); return 0;
        case WM_CLOSE: d->finish(false); return 0;
        case WM_DESTROY: d->done_=true; return 0;
        case WM_ERASEBKGND: return 1;
        case WM_MEASUREITEM: { auto* mi=reinterpret_cast<MEASUREITEMSTRUCT*>(l); mi->itemHeight=px(48,d->dpi); return TRUE; }
        case WM_DRAWITEM: d->drawControl(reinterpret_cast<DRAWITEMSTRUCT*>(l)); return TRUE;
        case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN: {
            auto* c=d->control(reinterpret_cast<HWND>(l)); HDC dc=reinterpret_cast<HDC>(w); COLORREF bg=c?(c->role==Role::Input||c->role==Role::List||c->role==Role::Combo?d->palette_.field:d->backing(*c)):d->palette_.surface;
            SetBkColor(dc,bg); SetTextColor(dc,c&&c->role==Role::Muted?d->palette_.muted:d->palette_.text); SetDCBrushColor(dc,bg); return reinterpret_cast<LRESULT>(GetStockObject(DC_BRUSH));
        }
        case WM_LBUTTONDOWN: { POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)}; for(auto& c:d->controls_) if(c.role==Role::Input) { RECT r{px(c.x,d->dpi),px(c.y,d->dpi),px(c.x+c.w,d->dpi),px(c.y+c.h,d->dpi)}; if(PtInRect(&r,p)) { SetFocus(c.handle); return 0; } } break; }
        case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT r{}; GetClientRect(h,&r); HDC memory=CreateCompatibleDC(dc); HBITMAP image=CreateCompatibleBitmap(dc,r.right,r.bottom); auto old=SelectObject(memory,image); d->paint(memory); BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY); SelectObject(memory,old); DeleteObject(image); DeleteDC(memory); EndPaint(h,&ps); return 0; }
    } } catch(const std::exception& e) { try { showNotice(h,L"操作未完成",utf16(e.what())); } catch(...) { MessageBoxW(h,L"暂时无法完成此操作，请重试。",L"ClassRollCall",MB_OK|MB_ICONWARNING); } return 0; }
    return DefWindowProcW(h,m,w,l);
}
DialogWindow::~DialogWindow() { if(hwnd&&IsWindow(hwnd)) DestroyWindow(hwnd); for(HFONT f:{font_,titleFont_,smallFont_}) if(f) DeleteObject(f); }
bool DialogWindow::show(HWND owner,const std::wstring& title) {
    static bool registered=[] { WNDCLASSEXW wc{sizeof wc}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.lpszClassName=L"ClassRollCall.Native.Dialog"; return RegisterClassExW(&wc)!=0; }();
    if(!registered) throw std::runtime_error("无法注册设置窗口"); auto monitor=MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST); dpi=monitorDpi(monitor); auto work=workArea(monitor);
    width=std::min(width,MulDiv(work.right-work.left,96,dpi)-32); height=std::min(height,MulDiv(work.bottom-work.top,96,dpi)-52);
    controls_.reserve(48); dark_=darkTheme(); palette_=Palette::system(dark_);
    HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,L"ClassRollCall.Native.Dialog",title.c_str(),WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,work.left+px(16,dpi),work.top+px(16,dpi),px(width,dpi),px(height,dpi),owner,nullptr,GetModuleHandleW(nullptr),this);
    if(!h) throw std::runtime_error("无法打开设置窗口"); styleTitleBar(h,dark_); layout(); create(); layout();
    RECT r{}; GetWindowRect(h,&r); SetWindowPos(h,HWND_TOP,work.left+(work.right-work.left-(r.right-r.left))/2,work.top+(work.bottom-work.top-(r.bottom-r.top))/2,0,0,SWP_NOSIZE);
    bool wasEnabled=owner&&IsWindowEnabled(owner); if(wasEnabled) EnableWindow(owner,FALSE); ShowWindow(h,SW_SHOW); SetForegroundWindow(h);
    HWND first=GetNextDlgTabItem(h,nullptr,FALSE); if(first) SetFocus(first); MSG message{}; bool quit=false;
    while(!done_) { BOOL got=GetMessageW(&message,nullptr,0,0); if(got<=0) { quit=got==0; break; } if(!IsDialogMessageW(h,&message)) { TranslateMessage(&message); DispatchMessageW(&message); } }
    if(IsWindow(h)) DestroyWindow(h); if(wasEnabled) { EnableWindow(owner,TRUE); SetForegroundWindow(owner); } if(quit) PostQuitMessage(static_cast<int>(message.wParam)); return accepted_;
}
}
