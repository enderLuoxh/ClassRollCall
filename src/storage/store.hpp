#pragma once
#include "core/random.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace crc {
struct ClassRoom { std::wstring id, name; std::vector<std::wstring> names; };
struct State {
    std::vector<ClassRoom> classes;
    std::wstring current, monitor;
    PickMode mode=PickMode::Balanced;
    int x=-1,y=-1;
    bool autostart=false, initialized=false;
    ClassRoom* active();
};
class Store {
    std::filesystem::path root_;
public:
    Store();
    explicit Store(std::filesystem::path root):root_(std::move(root)){}
    State load();
    void save(const State& state);
    const std::filesystem::path& root() const { return root_; }
};
std::string readBytes(const std::filesystem::path& path,size_t limit=16*1024*1024);
void atomicWrite(const std::filesystem::path& path,const std::string& bytes);
std::wstring uniqueId();
}
