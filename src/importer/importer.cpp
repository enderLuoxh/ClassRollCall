#include <windows.h>
#include "importer.hpp"
#include "storage/store.hpp"
#include <algorithm>
#include <stdexcept>
namespace crc {
Document importFile(const std::filesystem::path& path) {
    auto bytes=readBytes(path); auto ext=path.extension().wstring(); std::transform(ext.begin(),ext.end(),ext.begin(),towlower);
    Document document;
    if(ext==L".txt"||ext==L".csv") document.sheets.push_back(parseDelimited(decodeText(bytes),ext==L".txt"));
    else if(ext==L".xlsx") document=importXlsx(bytes);
    else if(ext==L".docx") document=importDocx(bytes);
    else if(ext==L".xls") document=importXls(bytes);
    else throw std::runtime_error("请选择 TXT、CSV、XLS、XLSX 或 DOCX 文件");
    document.source=path.filename().wstring(); return document;
}
}
