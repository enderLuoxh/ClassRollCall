#pragma once
#include <windows.h>
#include <string>
#include <vector>
namespace crc {
void enableDpi();
UINT monitorDpi(HMONITOR monitor);
UINT windowDpi(HWND window);
int px(int dip, UINT dpi);
RECT workArea(HMONITOR monitor);
RECT clampRect(RECT rect, RECT work);
std::wstring monitorName(HMONITOR monitor);
HMONITOR findMonitor(const std::wstring& device);
std::wstring executablePath();
bool darkTheme();
std::wstring startupCommand();
std::wstring startupValue();
void setStartup(bool enabled);
std::wstring utf16(const std::string& text);
std::string utf8(const std::wstring& text);
std::wstring trim(std::wstring value);
}
