#pragma once
#include "storage/store.hpp"
#include "importer/importer.hpp"
#include <windows.h>
#include <optional>
namespace crc {
struct NoticeAction { int id; std::wstring label; };
int showNotice(HWND owner,const std::wstring& title,const std::wstring& message,const std::vector<NoticeAction>& actions={{IDOK,L"知道了"}});
bool manageClasses(HWND owner,State& state,int initialPage=201);
bool firstRun(HWND owner,State& state);
bool importFiles(HWND owner,State& state,const std::vector<std::filesystem::path>& paths={});
std::optional<ClassRoom> previewImport(HWND owner,Document document,const std::wstring& defaultName);
}
