#include "ui/application.hpp"
#include "ui/dialogs.hpp"
#include "platform/platform.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <exception>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    crc::enableDpi();
    HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\ClassRollCall.Native.v2");
    if(!mutex) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) { if(HWND other=FindWindowW(L"ClassRollCall.Native.Float",nullptr)) PostMessageW(other,WM_APP+2,0,0); CloseHandle(mutex); return 0; }
    Gdiplus::GdiplusStartupInput input; input.SuppressBackgroundThread=TRUE;
    Gdiplus::GdiplusStartupOutput output; ULONG_PTR token=0,hook=0;
    if(Gdiplus::GdiplusStartup(&token,&input,&output)!=Gdiplus::Ok) { CloseHandle(mutex); return 1; }
    output.NotificationHook(&hook);
    INITCOMMONCONTROLSEX controls{sizeof controls,ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls); CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int code=1;
    try { crc::Application app(instance); code=app.run(show); }
    catch(const std::exception& e) { try { crc::showNotice(nullptr,L"无法启动",crc::utf16(e.what())); } catch(...) { MessageBoxW(nullptr,L"ClassRollCall 暂时无法启动。",L"ClassRollCall",MB_ICONERROR|MB_OK); } }
    CoUninitialize(); output.NotificationUnhook(hook); Gdiplus::GdiplusShutdown(token); CloseHandle(mutex); return code;
}
