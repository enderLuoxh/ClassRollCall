#include "store.hpp"
#include "platform/platform.hpp"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <map>
#include <stdexcept>
namespace crc {
ClassRoom* State::active() { for(auto& c:classes) if(c.id==current) return &c; return nullptr; }
Store::Store() { wchar_t local[MAX_PATH]{}; if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA|CSIDL_FLAG_CREATE,nullptr,SHGFP_TYPE_CURRENT,local))) throw std::runtime_error("Cannot find local app data"); root_=std::filesystem::path(local)/L"ClassRollCall"; }
std::wstring uniqueId() { GUID id{}; if(FAILED(CoCreateGuid(&id))) throw std::runtime_error("Cannot generate class ID"); wchar_t s[40]{}; StringFromGUID2(id,s,40); std::wstring value(s); value.erase(0,1); value.pop_back(); return value; }
std::string readBytes(const std::filesystem::path& p,size_t limit) { std::ifstream f(p,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error("Cannot open file"); auto n=f.tellg(); if(n<0||static_cast<uint64_t>(n)>limit) throw std::runtime_error("File exceeds size limit"); std::string s(static_cast<size_t>(n),0); f.seekg(0); if(!s.empty()&&!f.read(s.data(),static_cast<std::streamsize>(s.size()))) throw std::runtime_error("Cannot read file"); return s; }
void atomicWrite(const std::filesystem::path& p,const std::string& bytes) {
    std::filesystem::create_directories(p.parent_path()); auto temp=p; temp+=L"."+uniqueId()+L".tmp";
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot write user data");
    DWORD written=0; BOOL ok=WriteFile(f,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr);
    ok=ok&&written==bytes.size(); if(ok) ok=FlushFileBuffers(f); CloseHandle(f);
    if(!ok||!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { DeleteFileW(temp.c_str()); throw std::runtime_error("Cannot commit user data"); }
}
static std::map<std::wstring,std::wstring> ini(const std::filesystem::path& p) {
    std::map<std::wstring,std::wstring> result; if(!std::filesystem::exists(p)) return result;
    std::wistringstream in(utf16(readBytes(p))); std::wstring line;
    while(std::getline(in,line)) { if(!line.empty()&&line.back()==L'\r') line.pop_back(); auto pos=line.find(L'='); if(pos!=line.npos) result[line.substr(0,pos)]=line.substr(pos+1); } return result;
}
static std::wstring safe(std::wstring s) { for(auto& ch:s) if(ch==L'\r'||ch==L'\n'||ch==L'\0') ch=L' '; return trim(s); }
State Store::load() {
    State s; auto settings=ini(root_/L"settings.ini"); s.initialized=settings[L"initialized"]==L"1";
    s.current=settings[L"current"]; s.monitor=settings[L"monitor"]; s.autostart=settings[L"autostart"]==L"1"; s.mode=settings[L"mode"]==L"random"?PickMode::Random:PickMode::Balanced;
    try { if(!settings[L"x"].empty()) s.x=std::stoi(settings[L"x"]); if(!settings[L"y"].empty()) s.y=std::stoi(settings[L"y"]); } catch(...) { s.x=s.y=-1; }
    auto classes=ini(root_/L"classes.ini");
    for(const auto& item:classes) {
        if(item.first.size()!=36||item.first.find_first_not_of(L"0123456789abcdefABCDEF-")!=std::wstring::npos) throw std::runtime_error("Invalid stored class ID");
        ClassRoom c{item.first,item.second,{}}; std::wistringstream in(utf16(readBytes(root_/L"rosters"/(c.id+L".txt")))); std::wstring name;
        while(std::getline(in,name)) { name=trim(name); if(!name.empty()) c.names.push_back(name); } s.classes.push_back(std::move(c));
    }
    if(!s.active()&&!s.classes.empty()) s.current=s.classes.front().id; return s;
}
void Store::save(const State& s) {
    // Copy-on-write roster filenames: the atomic index switch commits all roster edits together.
    // Existing IDs stay immutable; an edited roster receives a fresh ID before save.
    std::wstring index;
    for(const auto& c:s.classes) {
        std::wstring text; for(const auto& n:c.names) text+=safe(n)+L"\n";
        auto file=root_/L"rosters"/(c.id+L".txt");
        if(!std::filesystem::exists(file)||readBytes(file)!=utf8(text)) atomicWrite(file,utf8(text));
        index+=c.id+L"="+safe(c.name)+L"\n";
    }
    atomicWrite(root_/L"classes.ini",utf8(index));
    std::wstring settings=L"initialized="+std::to_wstring(s.initialized?1:0)+L"\ncurrent="+s.current+L"\nmode="+(s.mode==PickMode::Random?L"random":L"balanced")+L"\nmonitor="+s.monitor+L"\nx="+std::to_wstring(s.x)+L"\ny="+std::to_wstring(s.y)+L"\nautostart="+std::to_wstring(s.autostart?1:0)+L"\ntheme=system\n";
    atomicWrite(root_/L"settings.ini",utf8(settings));
}
}
