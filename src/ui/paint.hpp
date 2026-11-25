#pragma once
#include <windows.h>
#include <string>
namespace crc {
void paintCard(HWND window,UINT dpi,bool result,const std::wstring& title,const std::wstring& subtitle,BYTE opacity,bool dark);
}
