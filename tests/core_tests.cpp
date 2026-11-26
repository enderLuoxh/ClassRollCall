#include "core/random.hpp"
#include "storage/store.hpp"
#include "platform/platform.hpp"
#include <set>
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    try {
        crc::Picker p(42);
        for(int round=0;round<100;++round) { std::set<size_t> seen; for(int i=0;i<37;++i) seen.insert(p.pick(37,crc::PickMode::Balanced)); check(seen.size()==37,"Balanced round repeats"); }
        p.reset(); check(p.pick(1,crc::PickMode::Balanced)==0,"Single record");
        bool failed=false; try { p.pick(0,crc::PickMode::Random); } catch(const std::invalid_argument&) { failed=true; } check(failed,"Empty list must fail");
        size_t hits[5]{}; for(int i=0;i<50000;++i) ++hits[p.pick(5,crc::PickMode::Random)]; for(auto h:hits) check(h>9000&&h<11000,"Random distribution");
        for(UINT dpi:{96u,120u,144u,168u,192u}) check(crc::px(48,dpi)==static_cast<int>(dpi/2),"DPI conversion");
        auto r=crc::clampRect({-2500,-100,-2436,-36},{-1920,0,0,1080}); check(r.left==-1920&&r.top==0,"Negative monitor coordinates");
        auto root=std::filesystem::temp_directory_path()/(L"ClassRollCall-test-"+crc::uniqueId()); crc::Store store(root); crc::State s; s.initialized=true; s.classes.push_back({crc::uniqueId(),L"高二 2班",{L"张三",L"张三",L"李四"}}); s.current=s.classes[0].id; s.x=123; s.y=456; store.save(s); auto loaded=store.load(); check(loaded.active()&&loaded.active()->names==s.classes[0].names,"Store roundtrip preserves duplicate names"); check(loaded.x==123&&loaded.y==456,"Position roundtrip");
        auto roster=root/L"rosters"/(s.classes[0].id+L".txt");
        auto indexBefore=crc::readBytes(root/L"classes.ini");
        HANDLE lock=CreateFileW(roster.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(lock!=INVALID_HANDLE_VALUE,"Lock test roster");
        s.x=321; s.y=654;
        try { store.saveSettings(s); } catch(...) { CloseHandle(lock); throw; }
        CloseHandle(lock);
        loaded=store.load();
        check(loaded.x==321&&loaded.y==654,"Position saves while roster is locked");
        check(loaded.active()&&loaded.active()->names==s.classes[0].names,"Settings save preserves roster");
        check(crc::readBytes(root/L"classes.ini")==indexBefore,"Settings save leaves class index unchanged");
        auto primary=root/L"primary",fallback=root/L"fallback";
        crc::Store initial(primary);initial.save(s);
        auto before=crc::readBytes(primary/L"settings.ini");
        crc::Store automatic(std::vector<std::filesystem::path>{primary,fallback});auto working=automatic.load();
        HANDLE settingsLock=CreateFileW((primary/L"settings.ini").c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        check(settingsLock!=INVALID_HANDLE_VALUE,"Lock primary settings");
        working.x=777;working.mode=crc::PickMode::Random;
        try { automatic.saveSettings(working); } catch(...) {CloseHandle(settingsLock);throw;}
        CloseHandle(settingsLock);
        check(automatic.root()==fallback,"Locked settings switch to a writable profile");
        check(crc::readBytes(primary/L"settings.ini")==before,"Failed profile keeps original settings");
        crc::Store restarted(std::vector<std::filesystem::path>{primary,fallback});auto restored=restarted.load();
        check(restarted.root()==fallback&&restored.x==777&&restored.mode==crc::PickMode::Random,"Restart selects the latest successful profile");
        check(restored.active()&&restored.active()->names==s.classes[0].names,"Fallback migrates every roster record including duplicates");
        auto blocked=root/L"not-a-directory";crc::atomicWrite(blocked,"sentinel");
        crc::Store denied(std::vector<std::filesystem::path>{blocked,root/L"safe-profile"});denied.save(s);
        check(denied.root()==root/L"safe-profile","Invalid primary directory falls back without elevation");
        check(crc::readBytes(blocked)=="sentinel","Fallback never overwrites a blocking file");
        crc::Store unavailable(std::vector<std::filesystem::path>{blocked});bool unavailableReported=false;
        try {unavailable.save(s);}catch(const std::exception& e){unavailableReported=std::string(e.what()).find("Windows")!=std::string::npos;}
        check(unavailableReported,"All failed destinations report the path and Windows error");
        // This test owns a fresh GUID directory directly under the OS temporary directory.
        std::filesystem::remove_all(root);
        std::cout<<"core tests passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
