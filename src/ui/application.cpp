#include "application.hpp"
#include "paint.hpp"
#include "dialogs.hpp"
#include "menu.hpp"
#include "theme.hpp"
#include "platform/platform.hpp"
#include "platform/compat.hpp"
#include "edition.hpp"
#include <windowsx.h>
#include <algorithm>
#include <exception>
#include <stdexcept>
namespace crc {
constexpr UINT trayMessage=WM_APP+1,showMessage=WM_APP+2;
constexpr UINT animateTimer=1,longTimer=2;
static const wchar_t* resultClass=L"ClassRollCall.Native.Result";
void Application::fail(const std::exception& e) { showNotice(floating_,L"操作未完成",utf16(e.what())); }
void Application::save() { store_.save(state_); storageWarning_=false; storageNotice_.clear(); }
void Application::storageFailure(const std::exception& e) {
    storageNotice_=utf16(e.what());
    if(storageWarning_||!trayAdded_) return;
    storageWarning_=true; OutputDebugStringW(storageNotice_.c_str());
    NOTIFYICONDATAW notice=tray_;notice.uFlags=NIF_INFO;notice.dwInfoFlags=NIIF_WARNING|NIIF_NOSOUND;
    lstrcpynW(notice.szInfoTitle,L"部分设置暂未保存",64);
    lstrcpynW(notice.szInfo,L"点名仍可继续使用。最新设置暂存于本次运行，退出后可能恢复旧设置；请在可写的文件夹运行软件。",256);
    Shell_NotifyIconW(NIM_MODIFY,&notice);
}
FloatState Application::floatState() {
    // The spinner only belongs to the one second roll; afterwards the dice comes
    // back while the result card stays on screen.
    if(busy_) return floatProgress()<1.f?FloatState::Busy:FloatState::Idle;
    if(down_) return FloatState::Pressed;
    if(hover_) return FloatState::Hover;
    return FloatState::Idle;
}
float Application::floatProgress() {
    if(!busy_) return 0.f;
    auto elapsed=GetTickCount64()-start_;
    if(elapsed>=1000) return 1.f;
    return static_cast<float>(elapsed)/1000.f;
}
ResultText Application::resultText() {
    ResultText value; auto* c=state_.active();
    value.name=shown_;
    if(c) { value.className=c->name; value.total=static_cast<int>(c->names.size()); }
    value.index=state_.mode==PickMode::Balanced?static_cast<int>(lastPick_):0;
    return value;
}
void Application::renderButton() { paintFloating(floating_,dpi_,floatState(),floatProgress(),dark_,static_cast<BYTE>(busy_||down_?242:128)); }
void Application::renderResult(BYTE alpha,float scale) { SetWindowTextW(result_,shown_.c_str()); paintResult(result_,resultDpi_,resultText(),dark_,alpha,scale); }
void Application::place(bool reset) {
    HMONITOR monitor=reset?nullptr:findMonitor(state_.monitor);
    bool valid=monitor!=nullptr;
    if(!monitor) monitor=MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY);
    dpi_=monitorDpi(monitor); RECT work=workArea(monitor);
    SIZE size=metrics::floatingWindow(dpi_); int w=size.cx,h=size.cy;
    int x=(!reset&&valid&&state_.x>=0)?work.left+px(state_.x,dpi_):work.right-w-px(10,dpi_);
    int y=(!reset&&valid&&state_.y>=0)?work.top+px(state_.y,dpi_):work.bottom-h-px(10,dpi_);
    RECT r=clampRect({x,y,x+w,y+h},work);
    SetWindowPos(floating_,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton();
}
void Application::remember(bool notify) { RECT r{}; if(!GetWindowRect(floating_,&r)) return; auto monitor=MonitorFromWindow(floating_,MONITOR_DEFAULTTONEAREST); auto work=workArea(monitor); state_.monitor=monitorName(monitor); state_.x=MulDiv(r.left-work.left,96,static_cast<int>(dpi_)); state_.y=MulDiv(r.top-work.top,96,static_cast<int>(dpi_)); try { store_.saveSettings(state_);storageWarning_=false;storageNotice_.clear(); } catch(const std::exception& e) { if(notify) storageFailure(e); } }
void Application::addTray() { tray_.cbSize=sizeof tray_; tray_.hWnd=floating_; tray_.uID=1; tray_.uFlags=NIF_ICON|NIF_TIP|NIF_MESSAGE; tray_.uCallbackMessage=trayMessage; if(!tray_.hIcon) { HICON small=static_cast<HICON>(LoadImageW(instance_,MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR)); tray_.hIcon=small?small:makeTrayIcon(); } auto tip=std::wstring(edition::displayName())+L" 点名"; lstrcpynW(tray_.szTip,tip.c_str(),static_cast<int>(sizeof tray_.szTip/sizeof(wchar_t))); trayAdded_=Shell_NotifyIconW(trayAdded_?NIM_MODIFY:NIM_ADD,&tray_)!=FALSE; ++trayAttempts_; if(trayAdded_||trayAttempts_>=5) KillTimer(floating_,3); else SetTimer(floating_,3,2000,nullptr); }
void Application::startPick() {
    if(busy_) return;
    auto* c=state_.active();
    if(!c||c->names.empty()) { if(showNotice(floating_,L"先添加一份名单",L"导入班级名单后，就可以点击悬浮按钮开始抽取。",{{IDCANCEL,L"稍后添加"},{IDYES,L"导入名单"}})==IDYES) command(11); return; }
    size_t selected=picker_.pick(c->names.size(),state_.mode); lastPick_=picker_.position(); final_=c->names[selected]; shown_=c->names.front();
    auto monitor=MonitorFromWindow(floating_,MONITOR_DEFAULTTONEAREST); resultDpi_=monitorDpi(monitor); auto work=workArea(monitor);
    SIZE box=metrics::resultWindow(resultDpi_);
    int w=std::min(static_cast<int>(box.cx),static_cast<int>(work.right-work.left)),h=std::min(static_cast<int>(box.cy),static_cast<int>(work.bottom-work.top));
    SetWindowPos(result_,HWND_TOPMOST,work.left+(work.right-work.left-w)/2,work.top+(work.bottom-work.top-h)/2,w,h,SWP_NOACTIVATE);
    busy_=true; frame_=0; start_=GetTickCount64(); renderButton(); renderResult(245,0.94f); ShowWindow(result_,SW_SHOWNOACTIVATE); SetTimer(floating_,animateTimer,33,nullptr);
}
void Application::tick() {
    auto elapsed=GetTickCount64()-start_;
    if(elapsed<1000) {
        auto* c=state_.active(); if(c&&!c->names.empty()) shown_=c->names[(frame_++*7)%c->names.size()];
        float t=static_cast<float>(elapsed)/160.f; if(t>1.f) t=1.f;
        renderResult(245,0.94f+0.06f*(1.f-(1.f-t)*(1.f-t)));
        renderButton();
        return;
    }
    if(elapsed<1180) {
        shown_=final_; float t=static_cast<float>(elapsed-1000)/180.f; if(t>1.f) t=1.f;
        renderResult(245,1.03f-0.03f*t); renderButton(); return;
    }
    if(elapsed<4000) {
        shown_=final_; renderResult(245,1.f);
        KillTimer(floating_,animateTimer); SetTimer(floating_,animateTimer,static_cast<UINT>(4000-elapsed),nullptr); return;
    }
    if(elapsed<4240) {
        float t=static_cast<float>(elapsed-4000)/240.f;
        shown_=final_; renderResult(static_cast<BYTE>(245*(1.f-t)),1.f-0.02f*t);
        KillTimer(floating_,animateTimer); SetTimer(floating_,animateTimer,33,nullptr); return;
    }
    KillTimer(floating_,animateTimer); ShowWindow(result_,SW_HIDE); busy_=false; renderButton();
}
void Application::press(POINT p,bool touch) { if(down_) return; down_=true; drag_=long_=false; touch_=touch; press_=p; RECT r{}; GetWindowRect(floating_,&r); windowStart_={r.left,r.top}; if(!touch) SetCapture(floating_); SetTimer(floating_,longTimer,500,nullptr); renderButton(); }
void Application::move(POINT p) {
    if(!down_||long_) return; int dx=p.x-press_.x,dy=p.y-press_.y;
    if(!drag_&&dx*dx+dy*dy>px(6,dpi_)*px(6,dpi_)) { drag_=true; KillTimer(floating_,longTimer); }
    if(!drag_) return;
    int x=windowStart_.x+dx,y=windowStart_.y+dy;
    auto monitor=MonitorFromPoint(p,MONITOR_DEFAULTTONEAREST); UINT next=monitorDpi(monitor); SIZE box=metrics::floatingWindow(next); int w=box.cx,h=box.cy; auto work=workArea(monitor);
    auto r=clampRect({x,y,x+w,y+h},work); dpi_=next;
    SetWindowPos(floating_,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton();
}
void Application::release(POINT p) { if(!down_) return; bool click=!drag_&&!long_; bool moved=drag_; down_=false; KillTimer(floating_,longTimer); if(GetCapture()==floating_) ReleaseCapture(); if(touch_) suppressMouse_=GetTickCount64()+700; touch_=false; pointerId_=0; renderButton(); if(moved) remember(); if(click) { RECT r{}; GetWindowRect(floating_,&r); if(PtInRect(&r,p)) startPick(); } }
void Application::cancelPress() { down_=drag_=long_=touch_=false; pointerId_=0; KillTimer(floating_,longTimer); if(GetCapture()==floating_) ReleaseCapture(); renderButton(); }
void Application::menu(POINT p) {
    std::vector<menu::Entry> classes;
    for(size_t i=0;i<state_.classes.size()&&i<500;++i) {
        const auto& c=state_.classes[i];
        classes.push_back(menu::item(c.name+L" "+std::to_wstring(c.names.size())+L" 人",static_cast<int>(1000+i),c.id==state_.current));
    }
    if(classes.empty()) { auto empty=menu::item(L"还没有班级",0); empty.disabled=true; classes.push_back(empty); }
    std::vector<menu::Entry> items;
    items.push_back(menu::group(L"切换班级",std::move(classes)));
    items.push_back(menu::item(L"管理班级名单…",10));
    items.push_back(menu::item(L"导入名单…",11));
    items.push_back(menu::split());
    items.push_back(menu::group(L"抽取模式",{menu::item(L"均衡抽取",20,state_.mode==PickMode::Balanced),menu::item(L"完全随机",21,state_.mode==PickMode::Random)}));
    items.push_back(menu::item(L"开机自动启动",30,state_.autostart));
    items.push_back(menu::split());
    items.push_back(menu::item(L"重置悬浮按钮位置",31));
    items.push_back(menu::item(IsWindowVisible(floating_)?L"隐藏悬浮按钮":L"显示悬浮按钮",32));
    items.push_back(menu::split());
    items.push_back(menu::item(L"退出",99));
    int picked=menu::show(floating_,dpi_,dark_,items,p);
    if(picked) command(static_cast<UINT>(picked));
}
void Application::command(UINT id) {
    if(busy_&&((id>=1000&&id<1500)||id==10||id==11||id==20||id==21)) {
        KillTimer(floating_,animateTimer); ShowWindow(result_,SW_HIDE); busy_=false; renderButton();
    }
    if(id>=1000&&id<1000+state_.classes.size()) { state_.current=state_.classes[id-1000].id; picker_.reset(); store_.saveSettings(state_); }
    else switch(id) {
    case 10: if(manageClasses(floating_,state_)) { picker_.reset(); save(); } break;
    case 11: if(manageClasses(floating_,state_,202)) { picker_.reset(); save(); } break;
    case 20: case 21: state_.mode=id==20?PickMode::Balanced:PickMode::Random; picker_.reset(); store_.saveSettings(state_); break;
    case 30: setStartup(!state_.autostart); state_.autostart=!state_.autostart; store_.saveSettings(state_); break;
    case 31: place(true); remember(); break;
    case 32: if(IsWindowVisible(floating_)&&!trayAdded_) { trayAttempts_=0; addTray(); if(!trayAdded_) { showNotice(floating_,L"暂时无法隐藏",L"系统托盘尚未准备好，悬浮按钮将保留，避免隐藏后无法找回。"); break; } } ShowWindow(floating_,IsWindowVisible(floating_)?SW_HIDE:SW_SHOWNOACTIVATE); break;
    case 99: DestroyWindow(floating_); break;
    }
}
LRESULT Application::message(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(hwnd==result_) { if(msg==WM_NCHITTEST) return HTTRANSPARENT; if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE; if(msg==WM_PAINT) { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; } return DefWindowProcW(hwnd,msg,wp,lp); }
    static const UINT taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    if(msg==taskbarCreated) { trayAdded_=false; trayAttempts_=0; addTray(); return 0; }
    switch(msg) {
    case WM_COMMAND: command(LOWORD(wp)); return 0;
    case 0x02CC: return 0x1|0x8|0x10; // WM_TABLET_QUERYSYSTEMGESTURESTATUS: own the 500 ms hold gesture.
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case 0x024B: return 3; // WM_POINTERACTIVATE: PA_NOACTIVATE.
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(hwnd,&ps); EndPaint(hwnd,&ps); return 0; }
    case WM_LBUTTONDOWN: if(GetTickCount64()>=suppressMouse_ && (GetMessageExtraInfo()&0xFFFFFF80)!=0xFF515700) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ClientToScreen(hwnd,&p); press(p,false); } return 0;
    case WM_MOUSEMOVE: if(down_&&!touch_) { POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)}; ClientToScreen(hwnd,&p); move(p); } else if(!hover_) { hover_=true; TRACKMOUSEEVENT track{sizeof track,TME_LEAVE,hwnd,0}; TrackMouseEvent(&track); renderButton(); } return 0;
    case WM_MOUSELEAVE: if(hover_) { hover_=false; renderButton(); } return 0;
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
    case WM_TIMER: if(wp==3) { addTray(); return 0; } if(wp==animateTimer) tick(); else if(wp==longTimer) { KillTimer(hwnd,longTimer); if(down_&&!drag_) { long_=true; POINT p=press_; down_=false; touch_=false; pointerId_=0; if(GetCapture()==hwnd) ReleaseCapture(); suppressMouse_=GetTickCount64()+700; renderButton(); menu(p); } } return 0;
    case WM_DROPFILES: {
        auto drop=reinterpret_cast<HDROP>(wp); std::vector<std::filesystem::path> paths; UINT count=DragQueryFileW(drop,0xFFFFFFFF,nullptr,0);
        for(UINT i=0;i<count&&i<64;++i) { UINT n=DragQueryFileW(drop,i,nullptr,0); std::wstring path(n+1,0); DragQueryFileW(drop,i,path.data(),n+1); path.resize(n); paths.emplace_back(path); } DragFinish(drop);
        if(!busy_&&importFiles(hwnd,state_,paths)) { picker_.reset(); save(); } return 0;
    }
    case WM_DPICHANGED: { dpi_=HIWORD(wp); auto r=*reinterpret_cast<RECT*>(lp); SIZE box=metrics::floatingWindow(dpi_); int w=box.cx,h=box.cy; r.right=r.left+w; r.bottom=r.top+h; r=clampRect(r,workArea(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST))); SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,w,h,SWP_NOACTIVATE); renderButton(); return 0; }
    case WM_DISPLAYCHANGE: place(false); return 0;
    case WM_SETTINGCHANGE: dark_=state_.theme==L"dark"?true:state_.theme==L"light"?false:darkTheme(); place(false); if(busy_) renderResult(); return 0;
    case showMessage: place(false); ShowWindow(hwnd,SW_SHOWNOACTIVATE); return 0;
    case trayMessage: if(lp==WM_LBUTTONDBLCLK) command(32); else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU) { POINT p{}; GetCursorPos(&p); menu(p); } return 0;
    case WM_ENDSESSION: if(wp) { try { remember(false); } catch(...) { OutputDebugStringW(L"ClassRollCall: could not save position during shutdown.\n"); } } return 0;
    case WM_DESTROY: KillTimer(hwnd,3); try { remember(false); } catch(...) { OutputDebugStringW(L"ClassRollCall: could not save position during exit.\n"); } KillTimer(hwnd,animateTimer); KillTimer(hwnd,longTimer); if(trayAdded_) Shell_NotifyIconW(NIM_DELETE,&tray_); if(tray_.hIcon) DestroyIcon(tray_.hIcon); if(result_) DestroyWindow(result_); PostQuitMessage(0); return 0;
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
    state_=store_.load(); if(edition::seed(state_)) { try { save(); } catch(const std::exception& e) { storageFailure(e); } } dark_=state_.theme==L"dark"?true:state_.theme==L"light"?false:darkTheme();
    WNDCLASSEXW wc{sizeof wc}; HICON icon=LoadIconW(instance_,MAKEINTRESOURCEW(1));
    wc.hInstance=instance_; wc.lpfnWndProc=proc; wc.hCursor=LoadCursorW(nullptr,IDC_HAND); wc.hIcon=icon; wc.hIconSm=icon; wc.lpszClassName=edition::floatingClass();
    if(!RegisterClassExW(&wc)) throw std::runtime_error("Cannot register window"); wc.lpszClassName=resultClass; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassExW(&wc);
    floating_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_NOACTIVATE,edition::floatingClass(),edition::displayName(),WS_POPUP,0,0,metrics::floatingWindow(96).cx,metrics::floatingWindow(96).cy,nullptr,nullptr,instance_,this);
    result_=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE,resultClass,L"点名结果",WS_POPUP,0,0,metrics::resultWindow(96).cx,metrics::resultWindow(96).cy,nullptr,nullptr,instance_,this);
    if(!floating_||!result_) throw std::runtime_error("Cannot create window");
    // Win7 receives WM_TOUCH; on Win8+ WM_POINTER is preferred and promoted mouse input is filtered.
    if(!GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetPointerInfo")) RegisterTouchWindow(floating_,TWF_WANTPALM);
    place(false); addTray(); DragAcceptFiles(floating_,TRUE); ShowWindow(floating_,SW_SHOWNOACTIVATE);
    if(!storageNotice_.empty()) { std::runtime_error pending(utf8(storageNotice_));storageFailure(pending); }
    if(!state_.initialized) { firstRun(floating_,state_); try { save(); } catch(const std::exception& e) { storageFailure(e); } }
    if(state_.autostart&&startupValue()!=startupCommand()) {
        if(showNotice(floating_,L"更新开机启动位置",L"程序的位置已改变。将开机启动项更新为当前路径，登录后就能继续使用。",{{IDCANCEL,L"暂不更新"},{IDYES,L"更新位置"}})==IDYES) setStartup(true);
    }
    MSG msg{}; BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return result==-1?1:static_cast<int>(msg.wParam);
}
}




