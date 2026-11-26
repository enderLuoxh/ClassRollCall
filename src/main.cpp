#include "ui/application.hpp"
#include "ui/dialogs.hpp"
#include "ui/preview.hpp"
#include "platform/platform.hpp"
#include "edition.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <exception>
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    crc::enableDpi();
    // Developer aid: wWinMain has no console, so `--preview` renders the UI off
    // screen into PNG files instead of opening any window.
    int argc=0; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    std::wstring previewDir;
    for(int i=1;argv&&i<argc;++i) { std::wstring arg=argv[i]; if(arg==L"--preview"&&i+1<argc) previewDir=argv[i+1]; else if(arg.rfind(L"--preview=",0)==0) previewDir=arg.substr(10); }
    if(argv) LocalFree(argv);
    Gdiplus::GdiplusStartupInput input; input.SuppressBackgroundThread=TRUE;
    Gdiplus::GdiplusStartupOutput output; ULONG_PTR token=0,hook=0;
    if(Gdiplus::GdiplusStartup(&token,&input,&output)!=Gdiplus::Ok) return 1;
    output.NotificationHook(&hook);
    if(!previewDir.empty()) { int code=crc::preview(previewDir); output.NotificationUnhook(hook); Gdiplus::GdiplusShutdown(token); return code; }
    HANDLE mutex=CreateMutexW(nullptr,FALSE,crc::edition::mutexName());
    if(!mutex) { output.NotificationUnhook(hook); Gdiplus::GdiplusShutdown(token); return 1; }
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        HWND other=nullptr;
        for(int attempt=0;attempt<10&&!other;++attempt) { other=FindWindowW(crc::edition::floatingClass(),nullptr); if(!other) Sleep(100); }
        if(other) PostMessageW(other,WM_APP+2,0,0);
        else MessageBoxW(nullptr,L"点名器已有一个后台进程，但未找到悬浮按钮。请在任务管理器中结束旧的 ClassRollCall 进程后重新打开。",crc::edition::displayName(),MB_ICONINFORMATION|MB_OK);
        CloseHandle(mutex); output.NotificationUnhook(hook); Gdiplus::GdiplusShutdown(token); return 0;
    }
    INITCOMMONCONTROLSEX controls{sizeof controls,ICC_STANDARD_CLASSES|ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&controls); CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int code=1;
    try { crc::Application app(instance); code=app.run(show); }
    catch(const std::exception& e) { try { crc::showNotice(nullptr,L"无法启动",crc::utf16(e.what())); } catch(...) { MessageBoxW(nullptr,L"ClassRollCall 暂时无法启动。",L"ClassRollCall",MB_ICONERROR|MB_OK); } }
    catch(...) { MessageBoxW(nullptr,L"点名器启动失败，请重新打开。",crc::edition::displayName(),MB_ICONERROR|MB_OK); }
    CoUninitialize(); output.NotificationUnhook(hook); Gdiplus::GdiplusShutdown(token); CloseHandle(mutex); return code;
}
