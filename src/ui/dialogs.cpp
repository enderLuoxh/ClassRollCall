#include "dialogs.hpp"
#include "platform/platform.hpp"
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <stdexcept>
namespace crc {
static std::wstring text(HWND h) { int n=GetWindowTextLengthW(h); std::wstring s(n+1,0); GetWindowTextW(h,s.data(),n+1); s.resize(n); return s; }
static void warning(HWND h,const std::exception& e) { MessageBoxW(h,utf16(e.what()).c_str(),L"操作未完成",MB_OK|MB_ICONWARNING); }
class Dialog {
    struct Control { HWND handle; int x,y,w,h; };
    std::vector<Control> controls_;
    HFONT font_=nullptr;
    bool done_=false,accepted_=false;
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
        Dialog* d=reinterpret_cast<Dialog*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(m==WM_NCCREATE) { d=static_cast<Dialog*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams); d->hwnd=h; SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(d)); }
        if(d) try {
            switch(m) {
            case WM_COMMAND: d->event(LOWORD(w),HIWORD(w)); return 0;
            case WM_DPICHANGED: d->dpi=HIWORD(w); d->layout(reinterpret_cast<RECT*>(l)); return 0;
            case WM_CLOSE: d->finish(false); return 0;
            case WM_DESTROY: d->done_=true; return 0;
            }
        } catch(const std::exception& e) { warning(h,e); return 0; }
        return DefWindowProcW(h,m,w,l);
    }
protected:
    HWND hwnd=nullptr;
    UINT dpi=96;
    int width=680,height=460;
    virtual void create()=0;
    virtual void event(int id,int) { if(id==IDCANCEL) finish(false); }
    void finish(bool yes) { accepted_=yes; done_=true; DestroyWindow(hwnd); }
    HWND add(const wchar_t* cls,const std::wstring& title,DWORD style,int id,int x,int y,int w,int h) {
        HWND c=CreateWindowExW(wcscmp(cls,L"EDIT")==0||wcscmp(cls,L"LISTBOX")==0?WS_EX_CLIENTEDGE:0,cls,title.c_str(),WS_CHILD|WS_VISIBLE|style,px(x,dpi),px(y,dpi),px(w,dpi),px(h,dpi),hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!c) throw std::runtime_error("无法创建界面控件"); controls_.push_back({c,x,y,w,h}); SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE); return c;
    }
    HWND button(const std::wstring& title,int id,int x,int y,int w=120) { return add(L"BUTTON",title,WS_TABSTOP|BS_PUSHBUTTON,id,x,y,w,44); }
    void label(const std::wstring& title,int x,int y,int w,int h=24) { add(L"STATIC",title,SS_LEFT,0,x,y,w,h); }
    HWND item(int id) { return GetDlgItem(hwnd,id); }
    void layout(const RECT* suggested=nullptr) {
        HFONT newFont=CreateFontW(-px(14,dpi),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        for(auto& c:controls_) { SetWindowPos(c.handle,nullptr,px(c.x,dpi),px(c.y,dpi),px(c.w,dpi),px(c.h,dpi),SWP_NOZORDER); SendMessageW(c.handle,WM_SETFONT,reinterpret_cast<WPARAM>(newFont),TRUE); }
        if(font_) DeleteObject(font_); font_=newFont;
        RECT outer{0,0,px(width,dpi),px(height,dpi)};
        using Adjust=BOOL(WINAPI*)(LPRECT,DWORD,BOOL,DWORD,UINT);
        static auto adjust=reinterpret_cast<Adjust>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"AdjustWindowRectExForDpi"));
        DWORD style=WS_POPUP|WS_CAPTION|WS_SYSMENU;
        if(adjust) adjust(&outer,style,FALSE,WS_EX_DLGMODALFRAME,dpi); else AdjustWindowRectEx(&outer,style,FALSE,WS_EX_DLGMODALFRAME);
        RECT current{}; GetWindowRect(hwnd,&current); int x=suggested?suggested->left:current.left,y=suggested?suggested->top:current.top;
        RECT r=clampRect({x,y,x+outer.right-outer.left,y+outer.bottom-outer.top},workArea(MonitorFromPoint({x,y},MONITOR_DEFAULTTONEAREST)));
        SetWindowPos(hwnd,HWND_TOP,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);
    }
public:
    virtual ~Dialog(){ if(font_) DeleteObject(font_); }
    bool show(HWND owner,const std::wstring& title) {
        static bool registered=[] { WNDCLASSEXW wc{sizeof wc}; wc.lpfnWndProc=proc; wc.hInstance=GetModuleHandleW(nullptr); wc.hCursor=LoadCursorW(nullptr,IDC_ARROW); wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1); wc.lpszClassName=L"ClassRollCall.Native.Dialog"; return RegisterClassExW(&wc)!=0; }();
        if(!registered) throw std::runtime_error("无法注册设置窗口");
        auto monitor=MonitorFromWindow(owner,MONITOR_DEFAULTTONEAREST); dpi=monitorDpi(monitor); RECT work=workArea(monitor);
        width=std::min(width,MulDiv(work.right-work.left,96,dpi)-32); height=std::min(height,MulDiv(work.bottom-work.top,96,dpi)-52);
        HWND h=CreateWindowExW(WS_EX_DLGMODALFRAME,L"ClassRollCall.Native.Dialog",title.c_str(),WS_POPUP|WS_CAPTION|WS_SYSMENU,work.left+px(16,dpi),work.top+px(16,dpi),px(width,dpi),px(height,dpi),owner,nullptr,GetModuleHandleW(nullptr),this);
        if(!h) throw std::runtime_error("无法打开设置窗口");
        layout(); create(); layout(); RECT r{}; GetWindowRect(h,&r); SetWindowPos(h,HWND_TOP,work.left+(work.right-work.left-(r.right-r.left))/2,work.top+(work.bottom-work.top-(r.bottom-r.top))/2,0,0,SWP_NOSIZE);
        bool wasEnabled=IsWindowEnabled(owner)!=FALSE; EnableWindow(owner,FALSE); ShowWindow(h,SW_SHOW); SetForegroundWindow(h);
        HWND first=GetNextDlgTabItem(h,nullptr,FALSE); if(first) SetFocus(first);
        MSG message{}; bool quit=false;
        while(!done_) { BOOL got=GetMessageW(&message,nullptr,0,0); if(got<=0) { quit=got==0; break; } if(!IsDialogMessageW(h,&message)) { TranslateMessage(&message); DispatchMessageW(&message); } }
        if(IsWindow(h)) DestroyWindow(h); if(wasEnabled) EnableWindow(owner,TRUE); SetForegroundWindow(owner); if(quit) PostQuitMessage(static_cast<int>(message.wParam)); return accepted_;
    }
};
class Editor:public Dialog {
    ClassRoom value_;
    void create() override {
        label(L"班级名称",20,18,width-40); add(L"EDIT",value_.name,WS_TABSTOP|ES_AUTOHSCROLL,100,20,46,width-40,34); SendMessageW(item(100),EM_SETLIMITTEXT,100,0);
        label(L"每行一个姓名；同名记录会保留",20,90,width-40);
        std::wstring all; for(const auto& n:value_.names) all+=n+L"\r\n";
        add(L"EDIT",all,WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,101,20,120,width-40,height-192); SendMessageW(item(101),EM_SETLIMITTEXT,1024*1024,0);
        button(L"取消",IDCANCEL,width-280,height-58); button(L"保存",IDOK,width-144,height-58);
    }
    void event(int id,int code) override {
        if(id!=IDOK) { Dialog::event(id,code); return; }
        auto name=trim(text(item(100))); if(name.empty()) throw std::runtime_error("请输入班级名称");
        auto sheet=parseDelimited(text(item(101)),true); auto selected=selectNames(sheet,0,0,false);
        value_.name=name; value_.names=std::move(selected.names); value_.id=uniqueId(); finish(true);
    }
public:
    explicit Editor(ClassRoom value):value_(std::move(value)){}
    ClassRoom value(){return std::move(value_);}
};
class Preview:public Dialog {
    Document doc_;
    std::wstring className_;
    std::optional<ClassRoom> result_;
    size_t sheet_=0;
    void update(bool reset) {
        auto& sheet=doc_.sheets.at(sheet_);
        if(reset) {
            SendMessageW(item(102),CB_RESETCONTENT,0,0); SendMessageW(item(102),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"请选择姓名列")); size_t count=0; for(auto& r:sheet.rows) count=std::max(count,r.size());
            for(size_t c=0;c<count;++c) { auto label=L"第 "+std::to_wstring(c+1)+L" 列"; if(!sheet.rows.empty()&&c<sheet.rows[0].size()&&!sheet.rows[0][c].empty()) label+=L" · "+sheet.rows[0][c].substr(0,30); SendMessageW(item(102),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())); }
            auto detected=detectNames(sheet); SendMessageW(item(102),CB_SETCURSEL,detected.column<0?0:detected.column+1,0);
            SetWindowTextW(item(103),std::to_wstring(detected.recognized?detected.header:0).c_str());
        }
        SendMessageW(item(104),LB_RESETCONTENT,0,0);
        auto col=static_cast<int>(SendMessageW(item(102),CB_GETCURSEL,0,0))-1;
        if(col<0) { SetWindowTextW(item(105),L"未确定姓名列，请手动选择；原表前 30 行如下。"); size_t rows=0; for(auto& r:sheet.rows) { std::wstring line; for(auto& c:r) { if(!line.empty()) line+=L"  |  "; line+=c.substr(0,40); } SendMessageW(item(104),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str())); if(++rows>=30) break; } EnableWindow(item(IDOK),FALSE); return; }
        int skip=GetDlgItemInt(hwnd,103,nullptr,FALSE); if(skip<0||skip>static_cast<int>(sheet.rows.size())) throw std::runtime_error("跳过行数无效");
        auto selected=selectNames(sheet,col,skip,SendMessageW(item(106),BM_GETCHECK,0,0)==BST_CHECKED);
        auto summary=L"人数 "+std::to_wstring(selected.names.size())+L"　重复记录 "+std::to_wstring(selected.duplicates)+L"　空白记录 "+std::to_wstring(selected.blanks);
        SetWindowTextW(item(105),summary.c_str()); size_t n=0; for(auto& name:selected.names) { SendMessageW(item(104),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str())); if(++n>=500) break; }
        EnableWindow(item(IDOK),!selected.names.empty());
    }
    void create() override {
        label(doc_.source,20,14,width-40); label(L"班级名称",20,44,100); add(L"EDIT",className_,WS_TABSTOP|ES_AUTOHSCROLL,100,124,40,width-144,32); SendMessageW(item(100),EM_SETLIMITTEXT,100,0);
        label(L"工作表 / 内容",20,88,120); add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,101,150,82,width-170,230);
        for(auto& s:doc_.sheets) SendMessageW(item(101),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.name.c_str())); SendMessageW(item(101),CB_SETCURSEL,0,0);
        label(L"姓名列",20,128,100); add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,102,124,122,width-144,230);
        label(L"跳过前几行",20,168,110); add(L"EDIT",L"0",WS_TABSTOP|ES_NUMBER,103,134,162,60,32); SendMessageW(item(103),EM_SETLIMITTEXT,5,0);
        add(L"BUTTON",L"去除重复姓名（默认保留）",WS_TABSTOP|BS_AUTOCHECKBOX,106,220,158,width-240,40);
        add(L"STATIC",L"",SS_LEFT,105,20,204,width-40,32);
        add(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOINTEGRALHEIGHT,104,20,240,width-40,height-310);
        button(L"取消",IDCANCEL,width-280,height-58); button(L"确认导入",IDOK,width-144,height-58); update(true);
    }
    void event(int id,int code) override {
        if(id==101&&code==CBN_SELCHANGE) { sheet_=static_cast<size_t>(SendMessageW(item(101),CB_GETCURSEL,0,0)); update(true); }
        else if((id==102&&code==CBN_SELCHANGE)||(id==103&&code==EN_CHANGE)||id==106) { if(item(104)&&item(105)) update(false); }
        else if(id==IDOK) { auto name=trim(text(item(100))); if(name.empty()) throw std::runtime_error("请输入班级名称"); int col=static_cast<int>(SendMessageW(item(102),CB_GETCURSEL,0,0))-1; if(col<0) return; auto names=selectNames(doc_.sheets[sheet_],col,GetDlgItemInt(hwnd,103,nullptr,FALSE),SendMessageW(item(106),BM_GETCHECK,0,0)==BST_CHECKED).names; if(names.empty()) throw std::runtime_error("没有可导入的姓名"); result_=ClassRoom{uniqueId(),name,std::move(names)}; finish(true); }
        else Dialog::event(id,code);
    }
public:
    Preview(Document doc,std::wstring name):doc_(std::move(doc)),className_(std::move(name)) { width=720; height=540; }
    std::optional<ClassRoom> result(){return std::move(result_);}
};
std::optional<ClassRoom> previewImport(HWND owner,Document doc,const std::wstring& name) { if(doc.sheets.empty()) throw std::runtime_error("没有可预览的内容"); Preview p(std::move(doc),name); if(p.show(owner,L"导入预览")) return p.result(); return {}; }
static bool insertClass(HWND owner,State& state,ClassRoom value) {
    for(;;) {
        auto it=std::find_if(state.classes.begin(),state.classes.end(),[&](auto& c){return c.name==value.name;});
        if(it==state.classes.end()) { state.current=value.id; state.classes.push_back(std::move(value)); return true; }
        TASKDIALOG_BUTTON choices[]={{100,L"重命名后导入"},{101,L"替换原班级"}};
        TASKDIALOGCONFIG config{sizeof config}; config.hwndParent=owner; config.dwFlags=TDF_ALLOW_DIALOG_CANCELLATION|TDF_SIZE_TO_CONTENT; config.pszWindowTitle=L"班级名称已存在"; config.pszMainInstruction=L"请选择如何处理同名班级"; config.pszContent=value.name.c_str(); config.cButtons=2; config.pButtons=choices; config.dwCommonButtons=TDCBF_CANCEL_BUTTON; config.nDefaultButton=100; int chosen=IDCANCEL;
        TaskDialogIndirect(&config,&chosen,nullptr,nullptr);
        if(chosen==IDCANCEL) return false;
        if(chosen==101) { state.current=value.id; *it=std::move(value); return true; }
        if(chosen==100) { Editor editor(value); if(!editor.show(owner,L"修改班级名称")) return false; value=editor.value(); }
        else return false;
    }
}
static std::vector<std::filesystem::path> chooseFiles(HWND owner) {
    std::vector<wchar_t> buffer(65536,0); OPENFILENAMEW ofn{sizeof ofn}; ofn.hwndOwner=owner; ofn.lpstrFile=buffer.data(); ofn.nMaxFile=static_cast<DWORD>(buffer.size()); ofn.lpstrFilter=L"班级名单 (*.txt;*.csv;*.xls;*.xlsx;*.docx)\0*.txt;*.csv;*.xls;*.xlsx;*.docx\0所有文件\0*.*\0"; ofn.Flags=OFN_EXPLORER|OFN_ALLOWMULTISELECT|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&ofn)) { if(CommDlgExtendedError()) throw std::runtime_error("无法选择文件或选择的文件过多"); return {}; }
    std::vector<std::filesystem::path> paths; const wchar_t* p=buffer.data(); std::filesystem::path first=p; p+=wcslen(p)+1; if(!*p) paths.push_back(first); else while(*p) { paths.push_back(first/p); p+=wcslen(p)+1; } return paths;
}
bool importFiles(HWND owner,State& state,const std::vector<std::filesystem::path>& input) {
    auto paths=input.empty()?chooseFiles(owner):input; bool changed=false;
    for(const auto& path:paths) try { auto doc=importFile(path); auto preview=previewImport(owner,std::move(doc),path.stem().wstring()); if(preview) changed=insertClass(owner,state,std::move(*preview))||changed; } catch(const std::exception& e) { warning(owner,e); }
    return changed;
}
class Manager:public Dialog {
    State& state_; bool changed_=false;
    void refresh() { SendMessageW(item(100),LB_RESETCONTENT,0,0); for(auto& c:state_.classes) { auto s=c.name+L"（"+std::to_wstring(c.names.size())+L"人）"; SendMessageW(item(100),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str())); } SendMessageW(item(100),LB_SETCURSEL,0,0); }
    void create() override { label(L"班级名单",20,16,width-40); add(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,100,20,52,width-190,height-72); button(L"新增班级",101,width-154,52); button(L"编辑 / 重命名",102,width-154,106); button(L"删除班级",103,width-154,160); button(L"导入文件",104,width-154,214); button(L"粘贴名单",105,width-154,268); button(L"完成",IDOK,width-154,height-58); refresh(); }
    void event(int id,int code) override {
        int index=static_cast<int>(SendMessageW(item(100),LB_GETCURSEL,0,0));
        if(id==101||id==102||(id==100&&code==LBN_DBLCLK)) { bool addNew=id==101; if(!addNew&&(index<0||index>=static_cast<int>(state_.classes.size()))) return; Editor editor(addNew?ClassRoom{L"",L"新班级",{}}:state_.classes[index]); if(editor.show(hwnd,L"编辑班级名单")) { auto c=editor.value(); for(size_t n=0;n<state_.classes.size();++n) if((addNew||n!=static_cast<size_t>(index))&&state_.classes[n].name==c.name) throw std::runtime_error("班级名称已存在，请使用其他名称"); state_.current=c.id; if(addNew) state_.classes.push_back(std::move(c)); else state_.classes[index]=std::move(c); changed_=true; refresh(); } }
        else if(id==103&&index>=0) { if(MessageBoxW(hwnd,L"确定删除所选班级及其名单？",L"删除班级",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)==IDYES) { state_.classes.erase(state_.classes.begin()+index); if(!state_.active()) state_.current=state_.classes.empty()?L"":state_.classes.front().id; changed_=true; refresh(); } }
        else if(id==104) { changed_=importFiles(hwnd,state_)||changed_; refresh(); }
        else if(id==105) { auto content=clipboardText(hwnd); auto selected=previewImport(hwnd,Document{L"剪贴板",{parseDelimited(content)}},L"粘贴的班级"); if(selected) changed_=insertClass(hwnd,state_,std::move(*selected))||changed_; refresh(); }
        else if(id==IDOK) finish(true); else Dialog::event(id,code);
    }
public:
    explicit Manager(State& state):state_(state){}
    bool changed()const{return changed_;}
};
bool manageClasses(HWND owner,State& state) { Manager m(state); m.show(owner,L"管理班级名单"); return m.changed(); }
class Welcome:public Dialog {
    State& state_;
    void create() override { label(L"欢迎使用 ClassRollCall",24,24,width-48,34); label(L"导入班级名单后，点击悬浮按钮即可点名。",24,72,width-48); label(L"支持鼠标拖动、触摸拖动和长按菜单。",24,106,width-48); button(L"导入名单",101,24,158,150); add(L"BUTTON",L"登录 Windows 后自动启动",WS_TABSTOP|BS_AUTOCHECKBOX,102,24,224,width-48,44); button(L"完成设置",IDOK,width-180,height-60,150); }
    void event(int id,int code) override { if(id==101) importFiles(hwnd,state_); else if(id==IDOK) { bool enabled=SendMessageW(item(102),BM_GETCHECK,0,0)==BST_CHECKED; if(enabled) setStartup(true); state_.autostart=enabled; state_.initialized=true; finish(true); } else Dialog::event(id,code); }
public:
    explicit Welcome(State& state):state_(state){width=540;height=350;}
};
bool firstRun(HWND owner,State& state) { Welcome w(state); return w.show(owner,L"首次运行设置"); }
}
