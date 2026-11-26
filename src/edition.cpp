#include "edition.hpp"
namespace crc::edition {
const wchar_t* dataFolder() { return L"ClassRollCall"; }
const wchar_t* mutexName() { return L"Local\\ClassRollCall.Native.v2"; }
const wchar_t* floatingClass() { return L"ClassRollCall.Native.Float"; }
const wchar_t* displayName() { return L"ClassRollCall"; }
bool seed(State&) { return false; }
}
