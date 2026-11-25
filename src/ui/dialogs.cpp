#include "dialogs.hpp"
#include "dialog_window.hpp"
#include <shobjidl.h>
#include <wrl/client.h>
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
static void warning(HWND h,const std::exception& e) { showNotice(h,L"操作未完成",utf16(e.what())); }
using Dialog=DialogWindow;
class Notice:public Dialog {
    std::wstring title_,message_;
    std::vector<NoticeAction> actions_;
    int chosen_=IDCANCEL;
    void create() override {
        header(title_,L"ClassRollCall");
        label(message_,28,108,width-56,height-206);
        int total=static_cast<int>(actions_.size())*144+static_cast<int>(actions_.size()-1)*12;
        int x=width-28-total;
        for(size_t i=0;i<actions_.size();++i) { auto& a=actions_[i]; button(a.label,a.id,x,height-64,144,i+1==actions_.size()?Role::Primary:Role::Secondary); x+=156; }
    }
    void event(int id,int code) override {
        for(auto& a:actions_) if(a.id==id) { chosen_=id; finish(true); return; }
        Dialog::event(id,code);
    }
public:
    Notice(std::wstring title,std::wstring message,std::vector<NoticeAction> actions):title_(std::move(title)),message_(std::move(message)),actions_(std::move(actions)) { width=actions_.size()>2?620:560; height=340; }
    int chosen()const{return chosen_;}
};
int showNotice(HWND owner,const std::wstring& title,const std::wstring& message,const std::vector<NoticeAction>& actions) { Notice n(title,message,actions); n.show(owner,title); return n.chosen(); }
class Editor:public Dialog {
    ClassRoom value_;
    void create() override {
        header(L"编辑班级",L"整理好名单，下一次打开即可直接点名。");
        label(L"班级名称",28,108,width-56,24,Role::Muted); add(L"EDIT",value_.name,WS_TABSTOP|ES_AUTOHSCROLL,100,28,138,width-56,48); SendMessageW(item(100),EM_SETLIMITTEXT,100,0);
        label(L"学生名单",28,208,180); label(L"每行一个姓名 · 同名记录会保留",220,210,width-248,24,Role::Muted);
        std::wstring all; for(const auto& n:value_.names) all+=n+L"\r\n";
        add(L"EDIT",all,WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,101,28,244,width-56,height-344); SendMessageW(item(101),EM_SETLIMITTEXT,1024*1024,0);
        button(L"取消",IDCANCEL,width-312,height-64); button(L"保存名单",IDOK,width-168,height-64,140);
    }
    void event(int id,int code) override {
        if(id!=IDOK) { Dialog::event(id,code); return; }
        auto name=trim(text(item(100))); if(name.empty()) throw std::runtime_error("请输入班级名称");
        auto sheet=parseDelimited(text(item(101)),true); auto selected=selectNames(sheet,0,0,false);
        value_.name=name; value_.names=std::move(selected.names); value_.id=uniqueId(); finish(true);
    }
public:
    explicit Editor(ClassRoom value):value_(std::move(value)){width=760;height=580;}
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
        auto summary=std::to_wstring(selected.names.size())+L" 位学生  ·  "+std::to_wstring(selected.duplicates)+L" 条重复  ·  "+std::to_wstring(selected.blanks)+L" 条空白";
        SetWindowTextW(item(105),summary.c_str()); size_t n=0; for(auto& name:selected.names) { SendMessageW(item(104),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str())); if(++n>=500) break; }
        EnableWindow(item(IDOK),!selected.names.empty());
    }
    void create() override {
        header(L"导入班级名单",doc_.source);
        const int left=std::min(336,(width-84)/2),right=left+52,available=width-right-28;
        card(24,100,left,height-184); card(right,100,available,height-184);
        label(L"班级名称",40,112,left-32,22,Role::Muted); add(L"EDIT",className_,WS_TABSTOP|ES_AUTOHSCROLL,100,40,136,left-32,44); SendMessageW(item(100),EM_SETLIMITTEXT,100,0);
        label(L"工作表 / 文档内容",40,190,left-32,22,Role::Muted); add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,101,40,214,left-32,44);
        for(auto& s:doc_.sheets) SendMessageW(item(101),CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.name.c_str())); SendMessageW(item(101),CB_SETCURSEL,0,0);
        label(L"姓名所在列",40,268,left-32,22,Role::Muted); add(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,102,40,292,left-32,44);
        label(L"跳过表头行数",40,354,left-120,24,Role::Muted); add(L"EDIT",L"0",WS_TABSTOP|ES_NUMBER,103,left-48,342,64,44); SendMessageW(item(103),EM_SETLIMITTEXT,5,0);
        if(height>=540) label(L"确认姓名列后，右侧预览会即时更新。",40,416,left-32,50,Role::Muted);
        label(L"名单预览",right+20,116,available-40,26);
        add(L"STATIC",L"",SS_LEFT|SS_NOPREFIX,105,right+20,150,available-40,42,Role::Muted);
        add(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|LBS_NOINTEGRALHEIGHT,104,right+8,196,available-16,height-358);
        add(L"BUTTON",L"去除重复姓名",WS_TABSTOP|BS_AUTOCHECKBOX,106,right+20,height-148,available-40,48);
        label(L"仅导入右侧预览中的姓名",28,height-50,width-380,24,Role::Muted);
        button(L"取消",IDCANCEL,width-312,height-64); button(L"确认导入",IDOK,width-168,height-64,140); update(true);
    }
    void event(int id,int code) override {
        if(id==101&&code==CBN_SELCHANGE) { sheet_=static_cast<size_t>(SendMessageW(item(101),CB_GETCURSEL,0,0)); update(true); }
        else if((id==102&&code==CBN_SELCHANGE)||(id==103&&code==EN_CHANGE)||id==106) { if(item(104)&&item(105)) update(false); }
        else if(id==IDOK) { auto name=trim(text(item(100))); if(name.empty()) throw std::runtime_error("请输入班级名称"); int col=static_cast<int>(SendMessageW(item(102),CB_GETCURSEL,0,0))-1; if(col<0) return; auto names=selectNames(doc_.sheets[sheet_],col,GetDlgItemInt(hwnd,103,nullptr,FALSE),SendMessageW(item(106),BM_GETCHECK,0,0)==BST_CHECKED).names; if(names.empty()) throw std::runtime_error("没有可导入的姓名"); result_=ClassRoom{uniqueId(),name,std::move(names)}; finish(true); }
        else Dialog::event(id,code);
    }
public:
    Preview(Document doc,std::wstring name):doc_(std::move(doc)),className_(std::move(name)) { width=940; height=610; }
    std::optional<ClassRoom> result(){return std::move(result_);}
};
std::optional<ClassRoom> previewImport(HWND owner,Document doc,const std::wstring& name) { if(doc.sheets.empty()) throw std::runtime_error("没有可预览的内容"); Preview p(std::move(doc),name); if(p.show(owner,L"导入预览")) return p.result(); return {}; }
static bool insertClass(HWND owner,State& state,ClassRoom value) {
    for(;;) {
        auto it=std::find_if(state.classes.begin(),state.classes.end(),[&](auto& c){return c.name==value.name;});
        if(it==state.classes.end()) { state.current=value.id; state.classes.push_back(std::move(value)); return true; }
        int chosen=showNotice(owner,L"班级名称已存在",L"已经有一个“"+value.name+L"”。\n选择重命名，或替换原有名单。",{{IDCANCEL,L"取消"},{101,L"替换原班级"},{100,L"重命名导入"}});
        if(chosen==IDCANCEL) return false;
        if(chosen==101) { state.current=value.id; *it=std::move(value); return true; }
        if(chosen==100) { Editor editor(value); if(!editor.show(owner,L"修改班级名称")) return false; value=editor.value(); }
        else return false;
    }
}
static std::vector<std::filesystem::path> chooseFiles(HWND owner) {
    using Microsoft::WRL::ComPtr;
    ComPtr<IFileOpenDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) throw std::runtime_error("无法打开文件选择窗口");
    DWORD options=0; dialog->GetOptions(&options);
    dialog->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST|FOS_PATHMUSTEXIST|FOS_ALLOWMULTISELECT|FOS_NOCHANGEDIR);
    COMDLG_FILTERSPEC filters[]={{L"班级名单",L"*.txt;*.csv;*.xls;*.xlsx;*.docx"},{L"所有文件",L"*.*"}};
    dialog->SetFileTypes(2,filters); dialog->SetTitle(L"选择班级名单"); dialog->SetOkButtonLabel(L"预览名单");
    HRESULT result=dialog->Show(owner); if(result==HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    if(FAILED(result)) throw std::runtime_error("无法选择名单文件");
    ComPtr<IShellItemArray> items; if(FAILED(dialog->GetResults(&items))) throw std::runtime_error("无法读取所选文件");
    DWORD count=0; items->GetCount(&count); if(count>64) throw std::runtime_error("一次最多导入 64 个名单文件");
    std::vector<std::filesystem::path> paths;
    for(DWORD i=0;i<count;++i) { ComPtr<IShellItem> item; if(FAILED(items->GetItemAt(i,&item))) throw std::runtime_error("无法读取文件路径"); PWSTR path=nullptr; if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) throw std::runtime_error("请选择本机中的文件"); paths.emplace_back(path); CoTaskMemFree(path); }
    return paths;
}

bool importFiles(HWND owner,State& state,const std::vector<std::filesystem::path>& input) {
    auto paths=input.empty()?chooseFiles(owner):input; bool changed=false;
    for(const auto& path:paths) try { auto doc=importFile(path); auto preview=previewImport(owner,std::move(doc),path.stem().wstring()); if(preview) changed=insertClass(owner,state,std::move(*preview))||changed; } catch(const std::exception& e) { warning(owner,e); }
    return changed;
}
class Manager:public Dialog {
    State& state_; bool changed_=false;
    void refresh() { SendMessageW(item(100),LB_RESETCONTENT,0,0); for(auto& c:state_.classes) { auto s=c.name+L"\t"+std::to_wstring(c.names.size())+L" 位学生"; SendMessageW(item(100),LB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str())); } SendMessageW(item(100),LB_SETCURSEL,0,0); if(item(107)) ShowWindow(item(107),state_.classes.empty()?SW_SHOW:SW_HIDE); }
    void create() override {
        label(L"班级名单",28,22,width-388,36,Role::Title); label(L"为不同班级保存各自的名单。",28,64,width-388,24,Role::Muted);
        button(L"粘贴名单",105,width-328,24,140); button(L"导入文件",104,width-172,24,144,Role::Primary);
        card(24,104,width-48,height-204);
        add(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,100,32,116,width-64,height-224);
        add(L"STATIC",L"还没有班级，先导入一份名单吧",SS_LEFT|SS_NOPREFIX,107,44,156,width-88,28,Role::Muted);
        button(L"新增班级",101,28,height-64,112); button(L"编辑名单",102,152,height-64,112); button(L"删除",103,276,height-64,88,Role::Danger); button(L"完成",IDOK,width-168,height-64,140); refresh();
    }
    void event(int id,int code) override {
        int index=static_cast<int>(SendMessageW(item(100),LB_GETCURSEL,0,0));
        if(id==101||id==102||(id==100&&code==LBN_DBLCLK)) { bool addNew=id==101; if(!addNew&&(index<0||index>=static_cast<int>(state_.classes.size()))) return; Editor editor(addNew?ClassRoom{L"",L"新班级",{}}:state_.classes[index]); if(editor.show(hwnd,L"编辑班级名单")) { auto c=editor.value(); for(size_t n=0;n<state_.classes.size();++n) if((addNew||n!=static_cast<size_t>(index))&&state_.classes[n].name==c.name) throw std::runtime_error("班级名称已存在，请使用其他名称"); state_.current=c.id; if(addNew) state_.classes.push_back(std::move(c)); else state_.classes[index]=std::move(c); changed_=true; refresh(); } }
        else if(id==103&&index>=0) { if(showNotice(hwnd,L"删除这个班级？",L"“"+state_.classes[index].name+L"”将从班级列表移除。",{{IDCANCEL,L"保留班级"},{IDYES,L"确认删除"}})==IDYES) { state_.classes.erase(state_.classes.begin()+index); if(!state_.active()) state_.current=state_.classes.empty()?L"":state_.classes.front().id; changed_=true; refresh(); } }
        else if(id==104) { changed_=importFiles(hwnd,state_)||changed_; refresh(); }
        else if(id==105) { auto content=clipboardText(hwnd); auto selected=previewImport(hwnd,Document{L"剪贴板",{parseDelimited(content)}},L"粘贴的班级"); if(selected) changed_=insertClass(hwnd,state_,std::move(*selected))||changed_; refresh(); }
        else if(id==IDOK) finish(true); else Dialog::event(id,code);
    }
public:
    explicit Manager(State& state):state_(state){width=820;height=560;}
    bool changed()const{return changed_;}
};
bool manageClasses(HWND owner,State& state) { Manager m(state); m.show(owner,L"管理班级名单"); return m.changed(); }
class Welcome:public Dialog {
    State& state_;
    void create() override {
        header(L"欢迎使用 ClassRollCall",L"准备一份名单，即可开始点名。");
        card(24,116,width-48,142); label(L"先准备一份班级名单",44,134,width-88,28); label(L"从表格、文档或文本导入，也可以稍后添加。",44,172,width-88,24,Role::Muted);
        button(L"导入名单",101,width-196,202,152,Role::Primary);
        card(24,278,width-48,76); add(L"BUTTON",L"登录 Windows 后自动启动",WS_TABSTOP|BS_AUTOCHECKBOX,102,44,292,width-88,48);
        label(L"点按抽取 · 拖动放置 · 长按打开菜单",28,height-52,width-250,28,Role::Muted); button(L"开始使用",IDOK,width-172,height-64,144);
    }
    void event(int id,int code) override { if(id==101) importFiles(hwnd,state_); else if(id==IDOK) { bool enabled=SendMessageW(item(102),BM_GETCHECK,0,0)==BST_CHECKED; if(enabled) setStartup(true); state_.autostart=enabled; state_.initialized=true; finish(true); } else Dialog::event(id,code); }
public:
    explicit Welcome(State& state):state_(state){width=660;height=460;}
};
bool firstRun(HWND owner,State& state) { Welcome w(state); return w.show(owner,L"首次运行设置"); }
}
