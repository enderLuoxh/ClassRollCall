#pragma once
#include <miniz.h>
#include <pugixml.hpp>
#include <string>
#include <vector>
namespace crc {
class OfficeZip {
    mz_zip_archive zip_{};
public:
    explicit OfficeZip(const std::string& bytes);
    ~OfficeZip();
    OfficeZip(const OfficeZip&)=delete;
    std::string part(const std::string& name,bool optional=false);
};
void xmlLoad(pugi::xml_document& doc,const std::string& bytes);
std::string localName(const char* name);
pugi::xml_node child(pugi::xml_node node,const char* name);
std::vector<pugi::xml_node> children(pugi::xml_node node,const char* name);
std::string richText(pugi::xml_node node);
}
