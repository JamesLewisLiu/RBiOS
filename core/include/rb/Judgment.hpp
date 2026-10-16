#pragma once
#include <array>
#include <cstdint>

namespace rb {
// Numeric result codes from 0x1016AC10. Names describe its branch ranges;
// presentation labels and complete note-event transitions are not restored here.
enum class TimingClass : int32_t { inner = 0, middle = 1, outer = 2, ignore = 6 };
// Exact table order from 0x102385B0 / 0x102385F0, selected by 0x1016ACA0.
std::array<int32_t, 8> timingWindows(uint32_t mode, bool alternate);
TimingClass classifyTiming(int32_t relativeTick, uint32_t mode, bool alternate,
                           bool rejectOuter); // 0x1016AC10

// Typed fields replace selected offsets in the original x86 CNoteBase instance.
// They are deliberately independent of ARM64 pointer size and object layout.
struct NoteTiming {
    int32_t kind = 0;               // *(note + 160) + 288
    int32_t relativeTick = 0;       // note + 192
    int32_t durationTicks = 0;      // note + 296
    int32_t segmentElapsed = 0;     // note + 360
    int32_t segmentIndex = -1;      // note + 364
    int32_t segmentDuration = 0;    // note + 368
    bool ended = false;             // note + 376
    bool isHoldScoringTick() const; // 0x10158640
    bool atSwitchBoundary() const;  // 0x10158700
    bool expired() const;           // 0x101587C0
};
} // namespace rb
