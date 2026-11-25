#pragma once
#include "core/random.hpp"
#include "storage/store.hpp"
#include <windows.h>
#include <shellapi.h>
namespace crc {
class Application {
    HINSTANCE instance_;
    HWND floating_=nullptr,result_=nullptr;
    UINT dpi_=96,resultDpi_=96;
    Store store_;
    State state_;
    Picker picker_;
    NOTIFYICONDATAW tray_{};
    bool busy_=false,down_=false,drag_=false,long_=false,touch_=false,dark_=false;
    UINT32 pointerId_=0;
    POINT press_{},windowStart_{};
    ULONGLONG start_=0,suppressMouse_=0;
    std::wstring final_,shown_;
    size_t frame_=0;
    struct MenuItem { std::wstring label; bool checked=false,disabled=false,submenu=false; };
    std::vector<MenuItem> menuItems_;
    bool measureMenu(MEASUREITEMSTRUCT* item);
    bool drawMenu(DRAWITEMSTRUCT* item);
    void renderButton();
    void renderResult(BYTE alpha=245);
    void place(bool reset);
    void remember();
    void addTray();
    void startPick();
    void tick();
    void press(POINT point,bool touch);
    void move(POINT point);
    void release(POINT point);
    void cancelPress();
    void menu(POINT point);
    void command(UINT id);
    void save();
    void fail(const std::exception& error);
    LRESULT message(HWND window,UINT msg,WPARAM wp,LPARAM lp);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
public:
    explicit Application(HINSTANCE instance):instance_(instance){}
    int run(int show);
};
}
