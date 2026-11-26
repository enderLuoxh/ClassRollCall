#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
namespace crc {
using Row=std::vector<std::wstring>;
struct Sheet { std::wstring name; std::vector<Row> rows; };
struct Document { std::wstring source; std::vector<Sheet> sheets; };
struct NameColumn { int column=-1; size_t header=0; bool recognized=false; };
struct Selection { std::vector<std::wstring> names; size_t blanks=0,duplicates=0; };
constexpr size_t maxRows=10000,maxColumns=256,maxCells=100000;
std::wstring decodeText(const std::string& bytes);
Sheet parseDelimited(const std::wstring& text,bool forceLines=false);
Document importFile(const std::filesystem::path& path);
Document importXlsx(const std::string& bytes);
Document importDocx(const std::string& bytes);
Document importXls(const std::string& bytes);
NameColumn detectNames(const Sheet& sheet);
Selection selectNames(const Sheet& sheet,size_t column,size_t firstRow,bool deduplicate);
std::wstring clipboardText(HWND owner);
}
