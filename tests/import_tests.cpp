#include "importer/importer.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>
static void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==2,"Fixture directory required"); std::filesystem::path root=argv[1];
        std::vector<std::wstring> expected{L"张三",L"李四",L"张三",L"王五"};
        for(auto encoding:{L"utf-8",L"utf-8-sig",L"gbk",L"gb18030",L"utf-16",L"utf-16-be"}) {
            auto doc=crc::importFile(root/(std::wstring(encoding)+L".txt")); auto s=crc::selectNames(doc.sheets[0],0,0,false); check(s.names==expected,"Text encoding roundtrip"); check(s.duplicates==1,"Duplicate count");
        }
        for(auto file:{L"utf-8-sig.csv",L"gb18030.csv",L"多列 多工作表.xlsx",L"中文 旧格式.xls",L"表格.docx"}) {
            auto doc=crc::importFile(root/file); size_t index=std::wstring(file).find(L"xlsx")!=std::wstring::npos?1:0;
            auto detected=crc::detectNames(doc.sheets.at(index)); check(detected.recognized&&detected.column==1,"Recognize name column");
            auto selected=crc::selectNames(doc.sheets[index],detected.column,detected.header,false); check(selected.names==expected,"Office and CSV names"); check(crc::selectNames(doc.sheets[index],detected.column,detected.header,true).names.size()==3,"Optional deduplication");
        }
        auto paragraphs=crc::importFile(root/L"段落.docx"); check(crc::selectNames(paragraphs.sheets[0],0,0,false).names==expected,"DOCX paragraphs");
        auto quoted=crc::importFile(root/L"quoted.csv"); check(quoted.sheets[0].rows[1][1]==L"张,三"&&quoted.sheets[0].rows[2][1]==L"李\"四","Quoted CSV");
        auto shared=crc::importFile(root/L"shared-formula.xlsx"); check(shared.sheets[1].rows[1][0]==L"张三"&&shared.sheets[1].rows[2][0]==L"cached","Shared strings and cached formulas");
        auto multi=crc::parseDelimited(L"001\t张三\t男\r\n002\t李四\t女"); check(crc::detectNames(multi).column==-1,"Ambiguous columns must require user selection");
        check(crc::selectNames(multi,1,0,false).names.size()==2,"Manual column choice");
        auto one=crc::parseDelimited(L"  张三  \n\n",true); check(crc::selectNames(one,0,0,false).names==std::vector<std::wstring>{L"张三"},"Trim and empty rows");
        for(auto file:{L"broken.xlsx",L"bomb.docx",L"dtd.docx"}) { bool failed=false; try { crc::importFile(root/file); } catch(const std::exception&) { failed=true; } check(failed,"Unsafe or damaged file must be rejected"); }
        bool failed=false; try { crc::parseDelimited(L"姓名\n\"张三"); } catch(const std::exception&) { failed=true; } check(failed,"Unclosed quote");
        std::cout<<"import tests passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
