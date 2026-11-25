#include "ui/application.hpp"
#include "platform/platform.hpp"
#include <gdiplus.h>
#include <commctrl.h>
#include <exception>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    crc::enableDpi();
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\ClassRollCall.Native.v2");
    if(!mutex) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) { if(HWND other=FindWindowW(L"ClassRollCall.Native.Float",nullptr)) PostMessageW(other,WM_APP+2,0,0); CloseHandle(mutex); return 0; }
    Gdiplus::GdiplusStartupInput input; ULONG_PTR token=0;
    if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) { CloseHandle(mutex); return 1; }
    INITCOMMONCONTROLSEX controls{sizeof controls,ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls); CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int code=1;
    try { crc::Application app(instance); code=app.run(show); }
    catch(const std::exception& e) { MessageBoxW(nullptr,(L"无法启动：\n"+crc::utf16(e.what())).c_str(),L"ClassRollCall",MB_ICONERROR|MB_OK); }
    CoUninitialize(); Gdiplus::GdiplusShutdown(token); CloseHandle(mutex); return code;
}
