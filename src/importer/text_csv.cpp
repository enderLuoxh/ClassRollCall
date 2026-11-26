#include <windows.h>
#include "importer.hpp"
#include "platform/platform.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace crc {
static std::wstring fromCodePage(const std::string& bytes,UINT cp) {
    if(bytes.empty()) return {}; int n=MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),nullptr,0);
    if(!n) throw std::runtime_error("无法识别文件编码，请另存为 UTF-8 后导入"); std::wstring text(n,0); MultiByteToWideChar(cp,MB_ERR_INVALID_CHARS,bytes.data(),static_cast<int>(bytes.size()),text.data(),n); return text;
}
std::wstring decodeText(const std::string& bytes) {
    if(bytes.size()>=2 && ((static_cast<unsigned char>(bytes[0])==0xFF&&static_cast<unsigned char>(bytes[1])==0xFE)||(static_cast<unsigned char>(bytes[0])==0xFE&&static_cast<unsigned char>(bytes[1])==0xFF))) {
        if(bytes.size()%2) throw std::runtime_error("UTF-16 文件不完整"); bool le=static_cast<unsigned char>(bytes[0])==0xFF; std::wstring result;
        for(size_t i=2;i<bytes.size();i+=2) { unsigned a=static_cast<unsigned char>(bytes[i]),b=static_cast<unsigned char>(bytes[i+1]); result.push_back(static_cast<wchar_t>(le?a|(b<<8):b|(a<<8))); }
        utf8(result); return result;
    }
    std::string s=bytes; if(s.compare(0,3,"\xEF\xBB\xBF")==0) s.erase(0,3);
    try { return utf16(s); } catch(const std::exception&) { return fromCodePage(s,54936); }
}
Sheet parseDelimited(const std::wstring& text,bool forceLines) {
    if(text.size()>8*1024*1024) throw std::runtime_error("粘贴内容超过大小限制");
    wchar_t delimiter=0;
    if(!forceLines) {
        size_t best=0;
        for(wchar_t d:{L'\t',L',',L';'}) { bool quote=false; size_t count=0,lines=0; for(size_t i=0;i<text.size()&&lines<20;++i) { wchar_t c=text[i]; if(c==L'"') { if(quote&&i+1<text.size()&&text[i+1]==L'"') ++i; else quote=!quote; } else if(!quote) { if(c==d) ++count; if(c==L'\n') ++lines; } } if(count>best) { best=count; delimiter=d; } }
    }
    Sheet sheet{L"文本",{}}; Row row; std::wstring cell; bool quote=false,closed=false; size_t cells=0;
    auto pushCell=[&] { if(row.size()>=maxColumns||++cells>maxCells||cell.size()>32768) throw std::runtime_error("表格超过支持的行列或文本长度限制"); row.push_back(trim(cell)); cell.clear(); closed=false; };
    auto pushRow=[&] { pushCell(); if(sheet.rows.size()>=maxRows) throw std::runtime_error("名单最多支持 10000 行"); sheet.rows.push_back(std::move(row)); row.clear(); };
    for(size_t i=0;i<text.size();++i) {
        wchar_t c=text[i];
        if(!forceLines&&c==L'"') { if(quote&&i+1<text.size()&&text[i+1]==L'"') { cell+=L'"'; ++i; } else if(quote) { quote=false; closed=true; } else if(cell.empty()&&!closed) quote=true; else throw std::runtime_error("CSV 引号格式不正确"); }
        else if(!quote&&delimiter&&c==delimiter) pushCell();
        else if(!quote&&(c==L'\r'||c==L'\n')) { if(c==L'\r'&&i+1<text.size()&&text[i+1]==L'\n') ++i; pushRow(); }
        else { if(closed&&!iswspace(c)) throw std::runtime_error("CSV 引号后有多余内容"); cell+=c; }
    }
    if(quote) throw std::runtime_error("CSV 引号未闭合");
    if(!cell.empty()||!row.empty()||sheet.rows.empty()) pushRow(); return sheet;
}
NameColumn detectNames(const Sheet& sheet) {
    static const std::set<std::wstring> headers{L"姓名",L"学生姓名",L"考生姓名",L"名字",L"学生",L"name",L"student name"};
    for(size_t r=0;r<std::min(size_t{20},sheet.rows.size());++r) {
        int found=-1; for(size_t c=0;c<sheet.rows[r].size();++c) { auto s=trim(sheet.rows[r][c]); std::transform(s.begin(),s.end(),s.begin(),towlower); if(headers.count(s)) { if(found>=0) return {}; found=static_cast<int>(c); } }
        if(found>=0) return {found,r+1,true};
    }
    size_t cols=0; for(const auto& r:sheet.rows) cols=std::max(cols,r.size()); return cols==1?NameColumn{0,0,false}:NameColumn{};
}
Selection selectNames(const Sheet& sheet,size_t column,size_t firstRow,bool deduplicate) {
    Selection result; std::set<std::wstring> seen;
    for(size_t r=firstRow;r<sheet.rows.size();++r) {
        std::wstring name=column<sheet.rows[r].size()?trim(sheet.rows[r][column]):L"";
        if(name.empty()) { ++result.blanks; continue; }
        if(name.size()>256||name.find_first_of(L"\r\n\t")!=name.npos||name.find(L'\0')!=name.npos) throw std::runtime_error("姓名包含换行、制表符或超过 256 字符，请调整姓名列");
        bool duplicate=!seen.insert(name).second; if(duplicate) ++result.duplicates; if(!deduplicate||!duplicate) result.names.push_back(std::move(name));
    }
    return result;
}
std::wstring clipboardText(HWND owner) {
    if(!OpenClipboard(owner)) throw std::runtime_error("暂时无法读取剪贴板，请重试");
    struct Close { ~Close(){CloseClipboard();} } close;
    HANDLE data=GetClipboardData(CF_UNICODETEXT); if(!data) throw std::runtime_error("剪贴板中没有文本");
    SIZE_T bytes=GlobalSize(data); if(bytes>16*1024*1024) throw std::runtime_error("剪贴板文本过大");
    const auto* text=static_cast<const wchar_t*>(GlobalLock(data)); if(!text) throw std::runtime_error("无法读取剪贴板");
    size_t len=0,limit=bytes/sizeof(wchar_t); while(len<limit&&text[len]) ++len;
    std::wstring result(text,len); GlobalUnlock(data); return result;
}
}
