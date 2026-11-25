#include "random.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <windows.h>
#include <wincrypt.h>
namespace crc {
Picker::Picker() {
    HCRYPTPROV provider = 0;
    unsigned seed = 0;
    if (CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        const BOOL ok = CryptGenRandom(provider, sizeof seed, reinterpret_cast<BYTE*>(&seed));
        CryptReleaseContext(provider, 0);
        if (!ok) throw std::runtime_error("Cannot obtain system randomness");
    } else throw std::runtime_error("Cannot obtain system randomness");
    engine_.seed(seed);
}
void Picker::reset() { round_.clear(); next_ = 0; }
size_t Picker::pick(size_t count, PickMode mode) {
    if (!count) throw std::invalid_argument("Empty roster");
    if (mode == PickMode::Random) return std::uniform_int_distribution<size_t>(0, count - 1)(engine_);
    if (round_.size() != count || next_ == round_.size()) {
        round_.resize(count); std::iota(round_.begin(), round_.end(), size_t{0});
        for (size_t i = count; i > 1; --i)
            std::swap(round_[i - 1], round_[std::uniform_int_distribution<size_t>(0, i - 1)(engine_)]);
        next_ = 0;
    }
    return round_[next_++];
}
}
