#include "store.hpp"
#include "platform/platform.hpp"
#include "edition.hpp"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>
#include <algorithm>
namespace crc {
ClassRoom* State::active() { for(auto& c:classes) if(c.id==current) return &c; return nullptr; }
namespace {
std::wstring environment(const wchar_t* name) {
    std::wstring value(32768,0); DWORD n=GetEnvironmentVariableW(name,value.data(),static_cast<DWORD>(value.size()));
    if(!n||n>=value.size()) return {}; value.resize(n); return value;
}
std::vector<std::filesystem::path> defaultRoots() {
    std::vector<std::filesystem::path> paths;
    auto append=[&](std::filesystem::path path) { if(path.is_absolute()&&std::find(paths.begin(),paths.end(),path)==paths.end()) paths.push_back(std::move(path)); };
    auto local=environment(L"LOCALAPPDATA");
    if(local.empty()) { wchar_t known[MAX_PATH]{}; if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,known))) local=known; }
    if(!local.empty()) append(std::filesystem::path(local)/edition::dataFolder());
    append(std::filesystem::path(executablePath()).parent_path()/L"data"/edition::dataFolder());
    auto roaming=environment(L"APPDATA"); if(!roaming.empty()) append(std::filesystem::path(roaming)/edition::dataFolder());
    wchar_t documents[MAX_PATH]{};
    if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_PERSONAL,nullptr,SHGFP_TYPE_CURRENT,documents))) append(std::filesystem::path(documents)/L"ClassRollCallData"/edition::dataFolder());
    return paths;
}
[[noreturn]] void storageError(const wchar_t* action,const std::filesystem::path& path,DWORD error) {
    wchar_t message[512]{}; FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,message,512,nullptr);
    throw std::runtime_error(utf8(std::wstring(action)+L"失败（Windows 错误 "+std::to_wstring(error)+L"）。\n"+path.wstring()+L"\n"+trim(message)));
}
bool fileExists(const std::filesystem::path& path) {
    if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES) return true;
    DWORD error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND) return false;
    storageError(L"访问数据文件",path,error);
}
void directory(const std::filesystem::path& path) {
    if(path.empty()) return;
    DWORD attrs=GetFileAttributesW(path.c_str());
    if(attrs!=INVALID_FILE_ATTRIBUTES) { if(attrs&FILE_ATTRIBUTE_DIRECTORY) return; storageError(L"创建数据目录",path,ERROR_DIRECTORY); }
    DWORD error=GetLastError();
    if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND) storageError(L"访问数据目录",path,error);
    auto parent=path.parent_path(); if(parent!=path) directory(parent);
    if(!CreateDirectoryW(path.c_str(),nullptr)) { error=GetLastError(); DWORD current=GetFileAttributesW(path.c_str());if(error==ERROR_ALREADY_EXISTS&&current!=INVALID_FILE_ATTRIBUTES&&(current&FILE_ATTRIBUTE_DIRECTORY)) return; storageError(L"创建数据目录",path,error); }
}
ULONGLONG profileTime(const std::filesystem::path& root,bool& found) {
    ULONGLONG latest=0;
    for(auto name:{L"classes.ini",L"settings.ini"}) { WIN32_FILE_ATTRIBUTE_DATA info{};
        if(GetFileAttributesExW((root/name).c_str(),GetFileExInfoStandard,&info)&&!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)) {
            found=true; ULARGE_INTEGER stamp{};stamp.LowPart=info.ftLastWriteTime.dwLowDateTime;stamp.HighPart=info.ftLastWriteTime.dwHighDateTime;latest=std::max(latest,stamp.QuadPart);
        }
    }
    return latest;
}
}
Store::Store():Store(defaultRoots()){}
Store::Store(std::vector<std::filesystem::path> roots):candidates_(std::move(roots)) {
    if(candidates_.empty()) throw std::runtime_error("找不到可用的数据目录"); root_=readRoot_=candidates_.front();
}
std::wstring uniqueId() { GUID id{}; if(FAILED(CoCreateGuid(&id))) throw std::runtime_error("Cannot generate class ID"); wchar_t s[40]{}; StringFromGUID2(id,s,40); std::wstring value(s); value.erase(0,1); value.pop_back(); return value; }
std::string readBytes(const std::filesystem::path& p,size_t limit) {
    struct Handle { HANDLE value;~Handle(){if(value!=INVALID_HANDLE_VALUE) CloseHandle(value);} } file{CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr)};
    if(file.value==INVALID_HANDLE_VALUE) storageError(L"读取文件",p,GetLastError());
    LARGE_INTEGER size{};if(!GetFileSizeEx(file.value,&size)) storageError(L"读取文件大小",p,GetLastError());
    if(size.QuadPart<0||static_cast<ULONGLONG>(size.QuadPart)>limit) throw std::runtime_error(utf8(L"文件超过大小限制："+p.wstring()));
    std::string bytes(static_cast<size_t>(size.QuadPart),0);size_t offset=0;
    while(offset<bytes.size()) {DWORD got=0;if(!ReadFile(file.value,bytes.data()+offset,static_cast<DWORD>(std::min<size_t>(bytes.size()-offset,MAXDWORD)),&got,nullptr)) storageError(L"读取文件",p,GetLastError());if(!got) storageError(L"读取文件",p,ERROR_HANDLE_EOF);offset+=got;}
    return bytes;
}
void atomicWrite(const std::filesystem::path& p,const std::string& bytes) {
    if(bytes.size()>MAXDWORD) throw std::runtime_error("数据文件超过大小限制");
    directory(p.parent_path()); auto temp=p; temp+=L"."+uniqueId()+L".tmp";
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) storageError(L"写入名单或设置",p,GetLastError());
    DWORD written=0; BOOL ok=WriteFile(f,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);
    ok=ok&&written==bytes.size(); if(ok) ok=FlushFileBuffers(f); DWORD error=ok?ERROR_SUCCESS:GetLastError(); CloseHandle(f);
    if(ok) for(int attempt=0;attempt<4;++attempt) {
        if(MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return;
        error=GetLastError(); if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_LOCK_VIOLATION&&error!=ERROR_ACCESS_DENIED) break;
        if(attempt<3) Sleep(20u<<attempt);
    }
    DeleteFileW(temp.c_str()); storageError(L"保存名单或设置",p,error?error:ERROR_WRITE_FAULT);
}
static std::map<std::wstring,std::wstring> ini(const std::filesystem::path& p) {
    std::map<std::wstring,std::wstring> result; if(!fileExists(p)) return result;
    std::wistringstream in(utf16(readBytes(p))); std::wstring line;
    while(std::getline(in,line)) { if(!line.empty()&&line.back()==L'\r') line.pop_back(); auto pos=line.find(L'='); if(pos!=line.npos) result[line.substr(0,pos)]=line.substr(pos+1); } return result;
}
static std::wstring safe(std::wstring s) { for(auto& ch:s) if(ch==L'\r'||ch==L'\n'||ch==L'\0') ch=L' '; return trim(s); }
State Store::load() {
    ULONGLONG latest=0;bool anyProfile=false;
    for(const auto& candidate:candidates_) { bool found=false;auto stamp=profileTime(candidate,found);if(found&&(!anyProfile||stamp>=latest)) { anyProfile=true;latest=stamp;root_=readRoot_=candidate; } }
    if(!anyProfile) return State{};
    State s; auto settings=ini(root_/L"settings.ini"); s.initialized=settings[L"initialized"]==L"1";
    s.current=settings[L"current"]; s.monitor=settings[L"monitor"]; s.autostart=settings[L"autostart"]==L"1"; s.mode=settings[L"mode"]==L"random"?PickMode::Random:PickMode::Balanced;
    if(settings[L"theme"]==L"dark"||settings[L"theme"]==L"light") s.theme=settings[L"theme"];
    try { if(!settings[L"x"].empty()) s.x=std::stoi(settings[L"x"]); if(!settings[L"y"].empty()) s.y=std::stoi(settings[L"y"]); } catch(...) { s.x=s.y=-1; }
    auto classes=ini(root_/L"classes.ini");
    for(const auto& item:classes) {
        if(item.first.size()!=36||item.first.find_first_not_of(L"0123456789abcdefABCDEF-")!=std::wstring::npos) throw std::runtime_error("Invalid stored class ID");
        ClassRoom c{item.first,item.second,{}}; std::wistringstream in(utf16(readBytes(root_/L"rosters"/(c.id+L".txt")))); std::wstring name;
        while(std::getline(in,name)) { name=trim(name); if(!name.empty()) c.names.push_back(name); } s.classes.push_back(std::move(c));
    }
    if(!s.active()&&!s.classes.empty()) s.current=s.classes.front().id; return s;
}
static void writeProfile(const std::filesystem::path& root,const State& s,bool settingsOnly) {
    // Copy-on-write roster filenames: the atomic index switch commits all roster edits together.
    // Existing IDs stay immutable; an edited roster receives a fresh ID before save.
    std::wstring index;
    if(!settingsOnly) for(const auto& c:s.classes) {
        std::wstring text; for(const auto& n:c.names) text+=safe(n)+L"\n";
        auto file=root/L"rosters"/(c.id+L".txt");
        if(!fileExists(file)||readBytes(file)!=utf8(text)) atomicWrite(file,utf8(text));
        index+=c.id+L"="+safe(c.name)+L"\n";
    }
    if(!settingsOnly) atomicWrite(root/L"classes.ini",utf8(index));
    std::wstring settings=L"initialized="+std::to_wstring(s.initialized?1:0)+L"\ncurrent="+s.current+L"\nmode="+(s.mode==PickMode::Random?L"random":L"balanced")+L"\nmonitor="+s.monitor+L"\nx="+std::to_wstring(s.x)+L"\ny="+std::to_wstring(s.y)+L"\nautostart="+std::to_wstring(s.autostart?1:0)+L"\ntheme="+s.theme+L"\n";
    atomicWrite(root/L"settings.ini",utf8(settings));
}
void Store::write(const State& s,bool settingsOnly) {
    std::string failure;
    try { writeProfile(root_,s,settingsOnly&&readRoot_==root_&&fileExists(root_/L"classes.ini"));readRoot_=root_;return; }
    catch(const std::exception& e) { failure=e.what(); }
    for(const auto& candidate:candidates_) {
        if(candidate==root_) continue;
        try { writeProfile(candidate,s,false);root_=readRoot_=candidate;return; }
        catch(const std::exception& e) { failure=e.what(); }
    }
    throw std::runtime_error(failure+utf8(L"\n所有备用数据目录均未能保存；本次修改仍保留在当前程序内。"));
}
void Store::save(const State& s) { write(s,false); }
void Store::saveSettings(const State& s) { write(s,true); }
}
