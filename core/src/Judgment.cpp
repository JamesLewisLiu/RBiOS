#include "rb/Judgment.hpp"
#include <cstring>

namespace rb {
namespace {
// Original instructions add/subtract 32-bit registers. Avoid signed C++ overflow
// while retaining the same bit pattern, including malformed-state diagnostics.
int32_t signedBits(uint32_t bits) {
    int32_t result;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}
int32_t add32(int32_t a, int32_t b) {
    return signedBits(uint32_t(a) + uint32_t(b));
}
int32_t sub32(int32_t a, int32_t b) {
    return signedBits(uint32_t(a) - uint32_t(b));
}
} // namespace

std::array<int32_t, 8> timingWindows(uint32_t mode, bool alternate) {
    // Verified byte-for-byte against the source DLL in verify_original.py.
    constexpr int32_t normal[2][8] = {{9, 9, 5, 3, -2, -5, -9, -9}, {9, 9, 4, 1, -1, -4, -9, -9}};
    constexpr int32_t other[2][8] = {{9, 9, 5, 4, -3, -5, -9, -9}, {9, 9, 5, 2, -2, -5, -9, -9}};
    const auto &values = alternate ? other[mode <= 1 ? mode : 0] : normal[mode <= 1 ? mode : 0];
    std::array<int32_t, 8> out{};
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = values[i];
    return out;
}
TimingClass classifyTiming(int32_t tick, uint32_t mode, bool alternate, bool rejectOuter) {
    const auto w = timingWindows(mode, alternate);
    if (w[4] <= tick && tick <= w[3])
        return TimingClass::inner;
    if (w[5] <= tick && tick <= w[2])
        return TimingClass::middle;
    return rejectOuter ? TimingClass::ignore : TimingClass::outer;
}
bool NoteTiming::isHoldScoringTick() const {
    int32_t cadence, remaining;
    if (kind == 1) {
        cadence = sub32(-15, relativeTick);
        remaining = add32(relativeTick, durationTicks);
    } else if (kind == 4) {
        cadence = sub32(segmentElapsed, 15);
        remaining = sub32(segmentDuration, segmentElapsed);
    } else {
        return false;
    }
    return cadence >= 0 && remaining >= 15 && cadence % 10 == 0;
}
bool NoteTiming::atSwitchBoundary() const {
    if (kind != 4)
        return false;
    return (segmentIndex < 0 ? relativeTick : sub32(segmentDuration, segmentElapsed)) <= 0;
}
bool NoteTiming::expired() const {
    return ended || relativeTick < -1000;
}
} // namespace rb
