#include "office.hpp"
#include <stdexcept>
#include <cstring>
namespace crc {
OfficeZip::OfficeZip(const std::string& bytes) {
    if(!mz_zip_reader_init_mem(&zip_,bytes.data(),bytes.size(),0)) throw std::runtime_error("Office 文件损坏或不是有效 ZIP");
    try {
        mz_uint count=mz_zip_reader_get_num_files(&zip_); if(count>2048) throw std::runtime_error("压缩包条目过多");
        uint64_t total=0; for(mz_uint i=0;i<count;++i) { mz_zip_archive_file_stat s{}; if(!mz_zip_reader_file_stat(&zip_,i,&s)) throw std::runtime_error("无法读取压缩包索引"); total+=s.m_uncomp_size; if(s.m_uncomp_size>16*1024*1024||total>64*1024*1024) throw std::runtime_error("Office 解压大小超过限制"); if(s.m_bit_flag&1) throw std::runtime_error("不支持加密文件"); }
    } catch(...) { mz_zip_reader_end(&zip_); throw; }
}
OfficeZip::~OfficeZip(){mz_zip_reader_end(&zip_);}
std::string OfficeZip::part(const std::string& name,bool optional) {
    if(name.empty()||name.front()=='/'||name.find("..")!=name.npos||name.find('\\')!=name.npos||name.find(':')!=name.npos) throw std::runtime_error("Office 内部路径无效");
    int i=mz_zip_reader_locate_file(&zip_,name.c_str(),nullptr,0); if(i<0) { if(optional) return {}; throw std::runtime_error("Office 文件缺少必要内容："+name); }
    mz_zip_archive_file_stat s{}; if(!mz_zip_reader_file_stat(&zip_,i,&s)||s.m_uncomp_size>16*1024*1024) throw std::runtime_error("Office 内容过大");
    std::string data(static_cast<size_t>(s.m_uncomp_size),0); if(!mz_zip_reader_extract_to_mem(&zip_,i,data.data(),data.size(),0)) throw std::runtime_error("Office 内容校验失败"); return data;
}
void xmlLoad(pugi::xml_document& doc,const std::string& bytes) {
    // Include and reject DOCTYPE nodes. pugixml never resolves external entities or resources.
    auto result=doc.load_buffer(bytes.data(),bytes.size(),pugi::parse_default|pugi::parse_doctype);
    if(!result) throw std::runtime_error("XML 内容损坏");
    for(auto node:doc.children()) if(node.type()==pugi::node_doctype) throw std::runtime_error("不允许 XML DTD");
    struct Walk:pugi::xml_tree_walker { size_t count=0; bool for_each(pugi::xml_node& n) override { return ++count<=300000&&depth()<=64&&n.type()!=pugi::node_doctype; } } walk;
    if(!doc.traverse(walk)) throw std::runtime_error("XML 结构超过限制");
}
std::string localName(const char* name) { const char* p=strchr(name,':'); return p?p+1:name; }
pugi::xml_node child(pugi::xml_node node,const char* name) { for(auto n:node.children()) if(localName(n.name())==name) return n; return {}; }
std::vector<pugi::xml_node> children(pugi::xml_node node,const char* name) { std::vector<pugi::xml_node> v; for(auto n:node.children()) if(localName(n.name())==name) v.push_back(n); return v; }
std::string richText(pugi::xml_node node) { std::string text; for(auto n:node.children()) { auto name=localName(n.name()); if(name=="t") text+=n.child_value(); else if(name=="tab") text+='\t'; else if(name=="br"||name=="cr") text+='\n'; else if(name!="rPh") text+=richText(n); if(text.size()>32768) throw std::runtime_error("单元格文本过长"); } return text; }
}
