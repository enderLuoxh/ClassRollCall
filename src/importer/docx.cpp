#include <windows.h>
#include "importer.hpp"
#include "office.hpp"
#include "platform/platform.hpp"
#include <stdexcept>
namespace crc {
Document importDocx(const std::string& bytes) {
    OfficeZip zip(bytes); pugi::xml_document xml; xmlLoad(xml,zip.part("word/document.xml"));
    Document doc; Sheet paragraphs{L"正文段落",{}}; size_t cells=0,tables=0;
    for(auto node:child(xml.document_element(),"body").children()) {
        auto type=localName(node.name());
        if(type=="p") { auto text=utf16(richText(node)); auto lines=parseDelimited(text); for(auto& row:lines.rows) { if(paragraphs.rows.size()>=maxRows) throw std::runtime_error("段落过多"); cells+=row.size(); paragraphs.rows.push_back(std::move(row)); } }
        else if(type=="tbl") {
            if(++tables>64) throw std::runtime_error("表格过多"); Sheet table{L"表格 "+std::to_wstring(tables),{}};
            for(auto tr:children(node,"tr")) { if(table.rows.size()>=maxRows) throw std::runtime_error("表格行数过多"); Row row; for(auto tc:children(tr,"tc")) { if(row.size()>=maxColumns) throw std::runtime_error("表格列数过多"); std::string text; for(auto p:children(tc,"p")) { if(!text.empty()) text+=' '; text+=richText(p); } row.push_back(trim(utf16(text))); ++cells; } table.rows.push_back(std::move(row)); }
            doc.sheets.push_back(std::move(table));
        }
        if(cells>maxCells) throw std::runtime_error("文档内容过多");
    }
    bool body=false; for(auto& row:paragraphs.rows) for(auto& cell:row) if(!cell.empty()) body=true;
    if(body||doc.sheets.empty()) doc.sheets.push_back(std::move(paragraphs)); return doc;
}
}
