#include <windows.h>
#include "importer.hpp"
#include "platform/platform.hpp"
#include <xls.h>
#include <memory>
#include <stdexcept>
namespace crc {
Document importXls(const std::string& bytes) {
    xls::xls_error_t error=xls::LIBXLS_OK;
    std::unique_ptr<xls::xlsWorkBook,decltype(&xls::xls_close_WB)> wb(xls::xls_open_buffer(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),"UTF-8",&error),&xls::xls_close_WB);
    if(!wb) throw std::runtime_error("XLS 读取失败："+std::string(xls::xls_getError(error)));
    auto decode=[&](const char* s) { if(!s) return std::wstring{}; if(!wb->is5ver) return utf16(s); int n=MultiByteToWideChar(wb->codepage,MB_ERR_INVALID_CHARS,s,-1,nullptr,0); if(!n) throw std::runtime_error("旧式 XLS 文本编码不受支持"); std::wstring w(n,0); MultiByteToWideChar(wb->codepage,MB_ERR_INVALID_CHARS,s,-1,w.data(),n); w.pop_back(); return w; };
    if(wb->sheets.count>64) throw std::runtime_error("工作表过多");
    Document doc; size_t cells=0;
    for(size_t i=0;i<wb->sheets.count;++i) {
        if(wb->sheets.sheet[i].type!=0) continue; // Ignore macro and chart sheets.
        std::unique_ptr<xls::xlsWorkSheet,decltype(&xls::xls_close_WS)> ws(xls::xls_getWorkSheet(wb.get(),static_cast<int>(i)),&xls::xls_close_WS);
        if(!ws||xls::xls_parseWorkSheet(ws.get())!=xls::LIBXLS_OK) throw std::runtime_error("XLS 工作表损坏");
        if(ws->rows.lastrow>=maxRows||ws->rows.lastcol>=maxColumns) throw std::runtime_error("XLS 行列超过支持范围");
        Sheet sheet{decode(wb->sheets.sheet[i].name),{}};
        for(size_t row=0;row<=ws->rows.lastrow;++row) { Row r; for(size_t col=0;col<=ws->rows.lastcol;++col) { auto* cell=xls::xls_cell(ws.get(),static_cast<xls::WORD>(row),static_cast<xls::WORD>(col)); std::wstring value=cell?trim(decode(cell->str)):L""; if(!value.empty()&&++cells>maxCells) throw std::runtime_error("XLS 单元格过多"); r.push_back(std::move(value)); } sheet.rows.push_back(std::move(r)); }
        doc.sheets.push_back(std::move(sheet));
    }
    if(doc.sheets.empty()) throw std::runtime_error("XLS 中没有可读取的工作表"); return doc;
}
}
