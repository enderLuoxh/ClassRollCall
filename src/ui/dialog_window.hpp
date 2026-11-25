#pragma once
#include "theme.hpp"
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
namespace crc {
class DialogWindow {
protected:
    enum class Role { Body, Title, Muted, Primary, Secondary, Danger, Input, Combo, List, Toggle, Navigation, SelectedNavigation };
private:
    struct Control { HWND handle; int x,y,w,h; Role role; bool multiline=false,hover=false; int group=0; };
    struct Card { RECT rect; int group; };
    std::vector<Control> controls_;
    std::vector<Card> cards_;
    int buildingGroup_=0,activeGroup_=0,shellWidth_=0;
    HFONT font_=nullptr,titleFont_=nullptr,smallFont_=nullptr;
    bool done_=false,accepted_=false,dark_=false;
    Palette palette_=Palette::system(false);
    Control* control(HWND h);
    COLORREF backing(const Control& c) const;
    void refreshTheme();
    void paint(HDC dc);
    void drawControl(DRAWITEMSTRUCT* draw);
    void drawToggle(HWND h,HDC dc);
    void drawClosedCombo(HWND h,HDC dc);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK childProc(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
protected:
    HWND hwnd=nullptr;
    UINT dpi=96;
    int width=760,height=520;
    virtual void create()=0;
    virtual void event(int id,int notification);
    void finish(bool accepted);
    HWND add(const wchar_t* cls,const std::wstring& title,DWORD style,int id,int x,int y,int w,int h,Role role=Role::Body);
    HWND button(const std::wstring& title,int id,int x,int y,int w=128,Role role=Role::Secondary);
    void label(const std::wstring& title,int x,int y,int w,int h=24,Role role=Role::Body);
    void header(const std::wstring& title,const std::wstring& subtitle);
    void card(int x,int y,int w,int h);
    void group(int id) { buildingGroup_=id; }
    void showGroup(int id);
    void role(int id,Role value);
    void settingsShell(int sidebar) { shellWidth_=sidebar; }
    HWND item(int id);
    void layout(const RECT* suggested=nullptr);
public:
    virtual ~DialogWindow();
    bool show(HWND owner,const std::wstring& title);
};
}
