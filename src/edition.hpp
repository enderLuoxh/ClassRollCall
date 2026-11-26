#pragma once
#include "storage/store.hpp"

namespace crc::edition {
const wchar_t* dataFolder();
const wchar_t* mutexName();
const wchar_t* floatingClass();
const wchar_t* displayName();
// Local custom builds seed an empty, separate profile with their built-in list.
bool seed(State& state);
}
