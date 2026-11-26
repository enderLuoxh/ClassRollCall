#include "platform.hpp"
#include "edition.hpp"
#include <algorithm>
#include <stdexcept>
#include <cwctype>
namespace crc {
void enableDpi() {
    HMODULE user = GetModuleHandleW(L"user32.dll");
    using Context = BOOL(WINAPI*)(HANDLE);
    if (auto f = reinterpret_cast<Context>(GetProcAddress(user, "SetProcessDpiAwarenessContext"))) {
        if (f(reinterpret_cast<HANDLE>(-4)) || GetLastError() == ERROR_ACCESS_DENIED) return;
    }
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        using Awareness = HRESULT(WINAPI*)(int);
        auto f = reinterpret_cast<Awareness>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        HRESULT hr = f ? f(2) : E_NOTIMPL;
        FreeLibrary(shcore);
        if (SUCCEEDED(hr) || hr == E_ACCESSDENIED) return;
    }
    SetProcessDPIAware();
}
UINT monitorDpi(HMONITOR monitor) {
    static HMODULE module = LoadLibraryW(L"shcore.dll");
    using Get = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    static auto get = module ? reinterpret_cast<Get>(GetProcAddress(module, "GetDpiForMonitor")) : nullptr;
    UINT x = 96, y = 96;
    if (get && SUCCEEDED(get(monitor, 0, &x, &y))) return x;
    HDC dc = GetDC(nullptr); x = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc);
    return x ? x : 96;
}
UINT windowDpi(HWND window) {
    using Get = UINT(WINAPI*)(HWND);
    static auto get = reinterpret_cast<Get>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    return get ? get(window) : monitorDpi(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST));
}
int px(int dip, UINT dpi) { return MulDiv(dip, static_cast<int>(dpi), 96); }
RECT workArea(HMONITOR monitor) { MONITORINFO info{sizeof info}; GetMonitorInfoW(monitor, &info); return info.rcWork; }
RECT clampRect(RECT r, RECT w) {
    int width = r.right - r.left, height = r.bottom - r.top;
    r.left = std::clamp(r.left, w.left, std::max(w.left, w.right - width));
    r.top = std::clamp(r.top, w.top, std::max(w.top, w.bottom - height));
    r.right = r.left + width; r.bottom = r.top + height; return r;
}
std::wstring monitorName(HMONITOR monitor) { MONITORINFOEXW info{}; info.cbSize=sizeof info; GetMonitorInfoW(monitor,&info); return info.szDevice; }
struct SearchMonitor { std::wstring device; HMONITOR found = nullptr; };
static BOOL CALLBACK findProc(HMONITOR m,HDC,LPRECT,LPARAM p) { auto& s=*reinterpret_cast<SearchMonitor*>(p); if(monitorName(m)==s.device) s.found=m; return TRUE; }
HMONITOR findMonitor(const std::wstring& name) { SearchMonitor s{name}; EnumDisplayMonitors(nullptr,nullptr,findProc,reinterpret_cast<LPARAM>(&s)); return s.found; }
std::wstring executablePath() { std::wstring s(32768,0); DWORD n=GetModuleFileNameW(nullptr,s.data(),static_cast<DWORD>(s.size())); if(!n || n>=s.size()) throw std::runtime_error("Cannot resolve executable path"); s.resize(n); return s; }
static constexpr wchar_t runKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
std::wstring startupCommand() { return L"\""+executablePath()+L"\""; }
std::wstring startupValue() {
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,runKey,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS) return {};
    DWORD type=0, bytes=0;
    LSTATUS st=RegQueryValueExW(key,edition::dataFolder(),nullptr,&type,nullptr,&bytes);
    std::wstring value(bytes/sizeof(wchar_t)+1,0);
    if(st==ERROR_SUCCESS && type==REG_SZ) st=RegQueryValueExW(key,edition::dataFolder(),nullptr,&type,reinterpret_cast<BYTE*>(value.data()),&bytes);
    RegCloseKey(key); if(st!=ERROR_SUCCESS || type!=REG_SZ) return {}; value.resize(wcslen(value.c_str())); return value;
}
void setStartup(bool enabled) {
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,runKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) throw std::runtime_error("Cannot open startup settings");
    std::wstring command=startupCommand();
    LSTATUS st=enabled?RegSetValueExW(key,edition::dataFolder(),0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),static_cast<DWORD>((command.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,edition::dataFolder());
    RegCloseKey(key); if(st!=ERROR_SUCCESS && !(st==ERROR_FILE_NOT_FOUND&&!enabled)) throw std::runtime_error("Cannot update startup settings");
}
bool darkTheme() {
    DWORD value=1,size=sizeof value;
    RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size); return value==0;
}
std::wstring utf16(const std::string& s) {
    if(s.empty()) return {}; int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n) throw std::runtime_error("Invalid UTF-8 text"); std::wstring w(n,0); MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n); return w;
}
std::string utf8(const std::wstring& w) {
    if(w.empty()) return {}; int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);
    if(!n) throw std::runtime_error("Invalid Unicode text"); std::string s(n,0); WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr); return s;
}
std::wstring trim(std::wstring s) { auto space=[](wchar_t c){return iswspace(c)||c==0xFEFF||c==0x200B;}; while(!s.empty()&&space(s.back())) s.pop_back(); auto i=std::find_if_not(s.begin(),s.end(),space); s.erase(s.begin(),i); return s; }
}
