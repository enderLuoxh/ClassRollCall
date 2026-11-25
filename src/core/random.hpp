#pragma once
#include <cstddef>
#include <random>
#include <vector>
namespace crc {
enum class PickMode { Balanced, Random };
class Picker {
    std::mt19937 engine_;
    std::vector<size_t> round_;
    size_t next_ = 0;
public:
    Picker();
    explicit Picker(unsigned seed) : engine_(seed) {}
    void reset();
    size_t pick(size_t count, PickMode mode);
};
}
