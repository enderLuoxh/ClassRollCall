#include <windows.h>
#include "importer.hpp"
#include "office.hpp"
#include "platform/platform.hpp"
#include <map>
#include <stdexcept>
namespace crc {
Document importXlsx(const std::string& bytes) {
    OfficeZip zip(bytes); pugi::xml_document workbook,rels,strings; xmlLoad(workbook,zip.part("xl/workbook.xml")); xmlLoad(rels,zip.part("xl/_rels/workbook.xml.rels"));
    std::map<std::string,std::string> targets;
    for(auto rel:rels.document_element().children()) { if(localName(rel.name())!="Relationship"||std::string(rel.attribute("TargetMode").value())=="External") continue; targets[rel.attribute("Id").value()]=rel.attribute("Target").value(); }
    std::vector<std::wstring> shared;
    auto content=zip.part("xl/sharedStrings.xml",true);
    if(!content.empty()) { xmlLoad(strings,content); for(auto si:children(strings.document_element(),"si")) { if(shared.size()>=maxCells) throw std::runtime_error("共享字符串过多"); shared.push_back(utf16(richText(si))); } }
    Document doc; size_t cells=0;
    for(auto s:children(child(workbook.document_element(),"sheets"),"sheet")) {
        if(doc.sheets.size()>=64) throw std::runtime_error("工作表过多");
        std::string id; for(auto a:s.attributes()) if(localName(a.name())=="id") id=a.value();
        auto it=targets.find(id); if(it==targets.end()) throw std::runtime_error("工作表关系缺失或引用外部内容");
        auto target=it->second; if(!target.empty()&&target.front()=='/') target.erase(0,1); else target="xl/"+target;
        pugi::xml_document xml; xmlLoad(xml,zip.part(target)); Sheet sheet{utf16(s.attribute("name").value()),{}};
        for(auto row:children(child(xml.document_element(),"sheetData"),"row")) {
            size_t rowIndex=row.attribute("r").as_uint(); if(rowIndex) --rowIndex; else rowIndex=sheet.rows.size();
            if(rowIndex>=maxRows) throw std::runtime_error("工作表超过 10000 行"); if(sheet.rows.size()<=rowIndex) sheet.rows.resize(rowIndex+1);
            size_t nextCol=0;
            for(auto c:children(row,"c")) {
                if(++cells>maxCells) throw std::runtime_error("工作簿非空单元格过多");
                std::string ref=c.attribute("r").value(); size_t col=0; bool letters=false;
                for(char ch:ref) { if(ch>='A'&&ch<='Z') { letters=true; col=col*26+(ch-'A'+1); if(col>maxColumns) throw std::runtime_error("姓名表最多支持 256 列"); } else break; }
                col=letters?col-1:nextCol; if(col>=maxColumns) throw std::runtime_error("列数超过限制"); nextCol=col+1;
                std::string type=c.attribute("t").value(); std::wstring value;
                if(type=="inlineStr") value=utf16(richText(child(c,"is")));
                else { auto v=child(c,"v"); if(type=="s"&&v) { size_t index=static_cast<size_t>(v.text().as_ullong(~0ULL)); if(index>=shared.size()) throw std::runtime_error("共享字符串索引无效"); value=shared[index]; } else if(v) value=utf16(v.child_value()); }
                auto& dest=sheet.rows[rowIndex]; if(dest.size()<=col) dest.resize(col+1); dest[col]=trim(value);
            }
        }
        doc.sheets.push_back(std::move(sheet));
    }
    if(doc.sheets.empty()) throw std::runtime_error("文件中没有工作表"); return doc;
}
}
