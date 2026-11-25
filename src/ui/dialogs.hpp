#pragma once
#include "storage/store.hpp"
#include "importer/importer.hpp"
#include <windows.h>
#include <optional>
namespace crc {
bool manageClasses(HWND owner,State& state);
bool firstRun(HWND owner,State& state);
bool importFiles(HWND owner,State& state,const std::vector<std::filesystem::path>& paths={});
std::optional<ClassRoom> previewImport(HWND owner,Document document,const std::wstring& defaultName);
}
