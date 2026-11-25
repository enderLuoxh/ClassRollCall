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
        // This test owns a fresh GUID directory directly under the OS temporary directory.
        std::filesystem::remove_all(root);
        std::cout<<"core tests passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
