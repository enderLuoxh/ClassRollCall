#include "application.hpp"
#include "paint.hpp"
#include "dialogs.hpp"
#include "theme.hpp"
#include "platform/platform.hpp"
#include <windowsx.h>
#include <algorithm>
#include <exception>
namespace crc {
constexpr UINT trayMessage=WM_APP+1,showMessage=WM_APP+2;
constexpr UINT animateTimer=1,longTimer=2;
static const wchar_t* floatingClass=L"ClassRollCall.Native.Float";
static const wchar_t* resultClass=L"ClassRollCall.Native.Result";
void Application::fail(const std::exception& e) { showNotice(floating_,L"操作未完成",utf16(e.what())); }
void Application::save() { store_.save(state_); }
void Application::renderButton() { paintCard(floating_,dpi_,false,L"Call",L"",busy_||down_?242:128,dark_); }
void Application::renderResult(BYTE alpha) { auto* c=state_.active(); SetWindowTextW(result_,shown_.c_str()); paintCard(result_,resultDpi_,true,shown_,c?c->name:L"",alpha,dark_); }
void Application::place(bool reset) {
    HMONITOR monitor=reset?nullptr:findMonitor(state_.monitor);
    bool valid=monitor!=nullptr;
    if(!monitor) monitor=MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY);
    dpi_=monitorDpi(monitor); RECT work=workArea(monitor); int w=px(108,dpi_),h=px(68,dpi_);
    int x=(!reset&&valid&&state_.x>=0)?work.left+px(state_.x,dpi_):work.right-w-px(24,dpi_);
    int y=(!reset&&valid&&state_.y>=0)?work.top+px(state_.y,dpi_):work.bottom-h-px(24,dpi_);
    RECT r=clampRect({x,y,x+w,y+h},work);
    SetWindowPos(floating_,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton();
}
void Application::remember() { RECT r{}; GetWindowRect(floating_,&r); auto monitor=MonitorFromWindow(floating_,MONITOR_DEFAULTTONEAREST); auto work=workArea(monitor); state_.monitor=monitorName(monitor); state_.x=MulDiv(r.left-work.left,96,static_cast<int>(dpi_)); state_.y=MulDiv(r.top-work.top,96,static_cast<int>(dpi_)); save(); }
void Application::addTray() { tray_.cbSize=sizeof tray_; tray_.hWnd=floating_; tray_.uID=1; tray_.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE; tray_.uCallbackMessage=trayMessage; if(!tray_.hIcon) tray_.hIcon=makeTrayIcon(); wcscpy_s(tray_.szTip,L"ClassRollCall · 点名"); if(!Shell_NotifyIconW(NIM_ADD,&tray_)) throw std::runtime_error("Cannot create tray icon"); }
void Application::startPick() {
    if(busy_) return;
    auto* c=state_.active();
    if(!c||c->names.empty()) { if(showNotice(floating_,L"先添加一份名单",L"导入班级名单后，就可以点击悬浮按钮开始抽取。",{{IDCANCEL,L"稍后添加"},{IDYES,L"导入名单"}})==IDYES) command(11); return; }
    size_t selected=picker_.pick(c->names.size(),state_.mode); final_=c->names[selected]; shown_=c->names.front();
    auto monitor=MonitorFromWindow(floating_,MONITOR_DEFAULTTONEAREST); resultDpi_=monitorDpi(monitor); auto work=workArea(monitor);
    int w=std::min(px(460,resultDpi_),static_cast<int>(work.right-work.left)),h=std::min(px(206,resultDpi_),static_cast<int>(work.bottom-work.top));
    SetWindowPos(result_,HWND_TOPMOST,work.left+(work.right-work.left-w)/2,work.top+(work.bottom-work.top-h)/2,w,h,SWP_NOACTIVATE);
    busy_=true; frame_=0; start_=GetTickCount64(); renderButton(); renderResult(); ShowWindow(result_,SW_SHOWNOACTIVATE); SetTimer(floating_,animateTimer,33,nullptr);
}
void Application::tick() {
    auto elapsed=GetTickCount64()-start_;
    if(elapsed<1000) { auto* c=state_.active(); if(c&&!c->names.empty()) shown_=c->names[(frame_++*7)%c->names.size()]; renderResult(); }
    else if(elapsed<4000) { shown_=final_; renderResult(); KillTimer(floating_,animateTimer); SetTimer(floating_,animateTimer,static_cast<UINT>(4000-elapsed),nullptr); }
    else if(elapsed<4240) { shown_=final_; renderResult(static_cast<BYTE>(245*(4240-elapsed)/240)); KillTimer(floating_,animateTimer); SetTimer(floating_,animateTimer,33,nullptr); }
    else { KillTimer(floating_,animateTimer); ShowWindow(result_,SW_HIDE); busy_=false; renderButton(); }
}
void Application::press(POINT p,bool touch) { if(down_) return; down_=true; drag_=long_=false; touch_=touch; press_=p; RECT r{}; GetWindowRect(floating_,&r); windowStart_={r.left,r.top}; if(!touch) SetCapture(floating_); SetTimer(floating_,longTimer,500,nullptr); renderButton(); }
void Application::move(POINT p) {
    if(!down_||long_) return; int dx=p.x-press_.x,dy=p.y-press_.y;
    if(!drag_&&dx*dx+dy*dy>px(6,dpi_)*px(6,dpi_)) { drag_=true; KillTimer(floating_,longTimer); }
    if(!drag_) return;
    int x=windowStart_.x+dx,y=windowStart_.y+dy;
    auto monitor=MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST); UINT next=monitorDpi(monitor); int w=px(108,next),h=px(68,next); auto work=workArea(monitor);
    auto r=clampRect({x,y,x+w,y+h},work); dpi_=next;
    SetWindowPos(floating_,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton();
}
void Application::release(POINT p) { if(!down_) return; bool click=!drag_&&!long_; bool moved=drag_; down_=false; KillTimer(floating_,longTimer); if(GetCapture()==floating_) ReleaseCapture(); if(touch_) suppressMouse_=GetTickCount64()+700; touch_=false; pointerId_=0; renderButton(); if(moved) remember(); if(click) { RECT r{}; GetWindowRect(floating_,&r); if(PtInRect(&r,p)) startPick(); } }
void Application::cancelPress() { down_=drag_=long_=touch_=false; pointerId_=0; KillTimer(floating_,longTimer); if(GetCapture()==floating_) ReleaseCapture(); renderButton(); }
bool Application::measureMenu(MEASUREITEMSTRUCT* item) {
    if(item->CtlType!=ODT_MENU||!item->itemData) return false;
    auto* value=reinterpret_cast<MenuItem*>(item->itemData); HDC dc=GetDC(floating_);
    HFONT font=CreateFontW(-px(15,dpi_),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); auto old=SelectObject(dc,font); SIZE size{}; GetTextExtentPoint32W(dc,value->label.c_str(),static_cast<int>(value->label.size()),&size); SelectObject(dc,old); DeleteObject(font); ReleaseDC(floating_,dc);
    item->itemHeight=px(48,dpi_); item->itemWidth=size.cx+px(78,dpi_); return true;
}
bool Application::drawMenu(DRAWITEMSTRUCT* item) {
    if(item->CtlType!=ODT_MENU||!item->itemData) return false;
    auto* value=reinterpret_cast<MenuItem*>(item->itemData); bool selected=(item->itemState&ODS_SELECTED)!=0;
    auto palette=Palette::system(dark_); HBRUSH brush=CreateSolidBrush(palette.surface); FillRect(item->hDC,&item->rcItem,brush); DeleteObject(brush);
    if(selected) { auto box=item->rcItem; InflateRect(&box,-px(4,dpi_),-px(3,dpi_)); roundedFill(item->hDC,box,px(8,dpi_),palette.selection,palette.selection); }
    SetBkMode(item->hDC,TRANSPARENT); SetTextColor(item->hDC,value->disabled?RGB(130,134,142):(dark_?RGB(241,243,249):RGB(35,46,65)));
    HFONT font=CreateFontW(-px(15,dpi_),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI"); auto old=SelectObject(item->hDC,font); auto rect=item->rcItem; rect.left+=px(34,dpi_); rect.right-=px(24,dpi_); DrawTextW(item->hDC,value->label.c_str(),-1,&rect,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
    if(value->checked) { auto mark=item->rcItem; mark.left+=px(8,dpi_); mark.right=mark.left+px(20,dpi_); DrawTextW(item->hDC,L"✓",-1,&mark,DT_SINGLELINE|DT_VCENTER|DT_CENTER); }
    if(value->submenu) { auto arrow=item->rcItem; arrow.left=arrow.right-px(24,dpi_); DrawTextW(item->hDC,L"›",-1,&arrow,DT_SINGLELINE|DT_VCENTER|DT_CENTER); }
    SelectObject(item->hDC,old); DeleteObject(font); return true;
}
void Application::menu(POINT p) {
    HMENU menu=CreatePopupMenu(),classes=CreatePopupMenu(),mode=CreatePopupMenu();
    menuItems_.clear(); menuItems_.reserve(std::min(state_.classes.size(),size_t{500})+20);
    auto append=[&](HMENU parent,UINT_PTR id,std::wstring label,bool checked=false,bool disabled=false,bool submenu=false) { menuItems_.push_back({std::move(label),checked,disabled,submenu}); AppendMenuW(parent,MF_OWNERDRAW|(disabled?MF_GRAYED:0)|(submenu?MF_POPUP:0),id,reinterpret_cast<LPCWSTR>(&menuItems_.back())); };
    for(size_t i=0;i<state_.classes.size()&&i<500;++i) { auto label=state_.classes[i].name+L"（"+std::to_wstring(state_.classes[i].names.size())+L"人）"; append(classes,1000+i,label,state_.classes[i].id==state_.current); }
    if(state_.classes.empty()) append(classes,0,L"尚无班级",false,true);
    append(menu,reinterpret_cast<UINT_PTR>(classes),L"切换班级",false,false,true);
    append(menu,10,L"管理班级名单"); append(menu,11,L"导入名单…");
    append(mode,20,L"均衡抽取",state_.mode==PickMode::Balanced); append(mode,21,L"完全随机",state_.mode==PickMode::Random);
    append(menu,reinterpret_cast<UINT_PTR>(mode),L"抽取模式",false,false,true);
    append(menu,30,L"开机自动启动",state_.autostart); append(menu,31,L"重置悬浮按钮位置");
    append(menu,32,IsWindowVisible(floating_)?L"隐藏悬浮按钮":L"显示悬浮按钮"); append(menu,40,L"关于"); AppendMenuW(menu,MF_SEPARATOR,0,nullptr); append(menu,99,L"退出");
    SetForegroundWindow(floating_); UINT id=TrackPopupMenuEx(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,floating_,nullptr); DestroyMenu(menu); PostMessageW(floating_,WM_NULL,0,0); if(id) command(id);
}
void Application::command(UINT id) {
    if(busy_&&((id>=1000&&id<1500)||id==10||id==11||id==20||id==21)) return;
    if(id>=1000&&id<1000+state_.classes.size()) { state_.current=state_.classes[id-1000].id; picker_.reset(); save(); }
    else switch(id) {
    case 10: if(manageClasses(floating_,state_)) { picker_.reset(); save(); } break;
    case 11: if(manageClasses(floating_,state_,202)) { picker_.reset(); save(); } break;
    case 20: case 21: state_.mode=id==20?PickMode::Balanced:PickMode::Random; picker_.reset(); save(); break;
    case 30: setStartup(!state_.autostart); state_.autostart=!state_.autostart; save(); break;
    case 31: place(true); remember(); break;
    case 32: ShowWindow(floating_,IsWindowVisible(floating_)?SW_HIDE:SW_SHOWNOACTIVATE); break;
    case 40: showNotice(floating_,L"ClassRollCall",L"版本 2.0 · 原生桌面点名器\n\n完全离线，轻量运行。\nCopyright © 2026 enderLuoxh\nMIT License"); break;
    case 99: DestroyWindow(floating_); break;
    }
}
LRESULT Application::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(hwnd==result_) { if(msg==WM_NCHITTEST) return HTTRANSPARENT; if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE; if(msg==WM_PAINT) { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; } return DefWindowProcW(hwnd,msg,wp,lp); }
    static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(msg==taskbarCreated) { addTray(); return 0; }
    switch(msg) {
    case WM_COMMAND: command(LOWORD(wp)); return 0;
    case WM_MEASUREITEM: if(measureMenu(reinterpret_cast<MEASUREITEMSTRUCT*>(lp))) return TRUE; break;
    case WM_DRAWITEM: if(drawMenu(reinterpret_cast<DRAWITEMSTRUCT*>(lp))) return TRUE; break;
    case 0x02CC: return 0x1|0x8|0x10; // WM_TABLET_QUERYSYSTEMGESTURESTATUS: own the 500 ms hold gesture.
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; }
    case WM_LBUTTONDOWN: if(GetTickCount64()>=suppressMouse_ && (GetMessageExtraInfo()&0xFFFFFF80)!=0xFF515700) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ClientToScreen(hwnd,&p); press(p,false); } return 0;
    case WM_MOUSEMOVE: if(down_&&!touch_) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ClientToScreen(hwnd,&p); move(p); } return 0;
    case WM_LBUTTONUP: if(down_&&!touch_) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ClientToScreen(hwnd,&p); release(p); } return 0;
    case WM_CAPTURECHANGED: if(down_&&!touch_) cancelPress(); return 0;
    case WM_CANCELMODE: cancelPress(); return 0;
    case WM_CONTEXTMENU: if(GetTickCount64()>=suppressMouse_) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; if(p.x==-1) GetCursorPos(&p); menu(p); } return 0;
    case WM_POINTERDOWN: case WM_POINTERUPDATE: case WM_POINTERUP: {
        using GetInfo=BOOL(WINAPI*)(UINT32,POINTER_INFO*);
        static auto get=reinterpret_cast<GetInfo>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetPointerInfo"));
        POINTER_INFO info{}; UINT32 id=GET_POINTERID_WPARAM(wp);
        if(get&&get(id,&info)&&info.pointerType==PT_TOUCH) {
            if(msg==WM_POINTERDOWN && !pointerId_) { pointerId_=id; press(info.ptPixelLocation,true); }
            else if(pointerId_==id) { if(info.pointerFlags&POINTER_FLAG_CANCELED) cancelPress(); else if(msg==WM_POINTERUP) release(info.ptPixelLocation); else move(info.ptPixelLocation); }
            suppressMouse_=GetTickCount64()+700; return 0;
        } break;
    }
    case WM_POINTERCAPTURECHANGED: cancelPress(); return 0;
    case WM_TOUCH: {
        UINT count=LOWORD(wp); std::vector<TOUCHINPUT> inputs(count); auto handle=reinterpret_cast<HTOUCHINPUT>(lp);
        BOOL ok=GetTouchInputInfo(handle,count,inputs.data(),sizeof(TOUCHINPUT)); CloseTouchInputHandle(handle);
        if(ok) for(const auto& t:inputs) { if(!(t.dwFlags&TOUCHEVENTF_PRIMARY)) continue; POINT p{TOUCH_COORD_TO_PIXEL(t.x),TOUCH_COORD_TO_PIXEL(t.y)}; if(t.dwFlags&TOUCHEVENTF_DOWN) press(p,true); else if(t.dwFlags&TOUCHEVENTF_UP) release(p); else move(p); }
        suppressMouse_=GetTickCount64()+700; return 0;
    }
    case WM_TIMER: if(wp==animateTimer) tick(); else if(wp==longTimer) { KillTimer(hwnd,longTimer); if(down_&&!drag_) { long_=true; POINT p=press_; down_=false; touch_=false; pointerId_=0; if(GetCapture()==hwnd) ReleaseCapture(); suppressMouse_=GetTickCount64()+700; renderButton(); menu(p); } } return 0;
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp); std::vector<std::filesystem::path> paths; UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
        for(UINT i=0;i<count&&i<64;++i) { UINT n=DragQueryFileW(drop,i,nullptr,0); std::wstring path(n+1,0); DragQueryFileW(drop,i,path.data(),n+1); path.resize(n); paths.emplace_back(path); } DragFinish(drop);
        if(!busy_&&importFiles(hwnd,state_,paths)) { picker_.reset(); save(); } return 0;
    }
    case WM_DPICHANGED: { dpi_=HIWORD(wp); auto r=*reinterpret_cast<RECT*>(lp); int w=px(108,dpi_),h=px(68,dpi_); r.right=r.left+w; r.bottom=r.top+h; r=clampRect(r,workArea(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST))); SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton(); return 0; }
    case WM_DISPLAYCHANGE: place(false); return 0;
    case WM_SETTINGCHANGE: dark_=darkTheme(); place(false); if(busy_) renderResult(); return 0;
    case showMessage: ShowWindow(hwnd,SW_SHOWNOACTIVATE); return 0;
    case trayMessage: if(lp==WM_LBUTTONDBLCLK) command(32); else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU) { POINT p{}; GetCursorPos(&p); menu(p); } return 0;
    case WM_ENDSESSION: if(wp) remember(); return 0;
    case WM_DESTROY: remember(); KillTimer(hwnd,animateTimer); KillTimer(hwnd,longTimer); Shell_NotifyIconW(NIM_DELETE,&tray_); if(tray_.hIcon) DestroyIcon(tray_.hIcon); if(result_) DestroyWindow(result_); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK Application::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* app=reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) { app=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams); SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app)); }
    if(app) { try { return app->message(hwnd,msg,wp,lp); } catch(const std::exception& e) { app->cancelPress(); app->fail(e); return 0; } }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int Application::run(int) {
    state_=store_.load(); dark_=darkTheme();
    WNDCLASSEXW wc{sizeof wc}; wc.hInstance=instance_; wc.lpfnWndProc=proc; wc.hCursor=LoadCursorW(nullptr,IDC_HAND); wc.lpszClassName=floatingClass;
    if(!RegisterClassExW(&wc)) throw std::runtime_error("Cannot register window"); wc.lpszClassName=resultClass; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassExW(&wc);
    floating_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_NOACTIVATE,floatingClass,L"ClassRollCall",WS_POPUP,0,0,108,68,nullptr,nullptr,instance_,this);
    result_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,resultClass,L"点名结果",WS_POPUP,0,0,460,206,nullptr,nullptr,instance_,this);
    if(!floating_||!result_) throw std::runtime_error("Cannot create window");
    // Win7 receives WM_TOUCH; on Win8+ WM_POINTER is preferred and promoted mouse input is filtered.
    if(!GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetPointerInfo")) RegisterTouchWindow(floating_,TWF_WANTPALM);
    place(false); addTray(); DragAcceptFiles(floating_,TRUE); ShowWindow(floating_,SW_SHOWNOACTIVATE);
    if(!state_.initialized) { firstRun(floating_,state_); save(); }
    if(state_.autostart&&startupValue()!=startupCommand()) {
        if(showNotice(floating_,L"更新开机启动位置",L"程序的位置已改变。将开机启动项更新为当前路径，登录后就能继续使用。",{{IDCANCEL,L"暂不更新"},{IDYES,L"更新位置"}})==IDYES) setStartup(true);
    }
    MSG msg{}; BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return result==-1?1:static_cast<int>(msg.wParam);
}
}
