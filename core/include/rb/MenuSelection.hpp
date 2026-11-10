#pragma once
#include "StageSound.hpp"
#include <array>
#include <cstdint>
namespace rb {
// CNoteGradeSelectWindow, 0x1011A170. The adapter supplies one input per 60 Hz tick.
struct DifficultyMenuInput {
    int32_t panel = -1;
    bool next = false, confirm = false, detail = false;
    bool debounceEnabled = true; // original window +84 is nonzero
};
struct DifficultyMenuResult {
    int32_t difficulty = 0;
    bool changed = false, confirmed = false, detailChanged = false;
};
struct DifficultyMenu {
    int32_t selected = 0, cooldown = 0;
    std::array<bool, 4> enabled{{true, true, true, true}};
    bool showDetail = false;
    DifficultyMenuResult update(const DifficultyMenuInput &input);
};
// Table 0x10292624, emitted only when 0x1011A170 changes selection.
StageSoundCue difficultyMenuSound(int32_t difficulty);
// 0x100ADBD0 compares the catalog's audio-suffix bytes, not level numbers.
bool sameDifficultyAudio(const std::array<uint8_t, 4> &suffix, int32_t a, int32_t b);
// 0x1011A380: upper difficulties require the recovered eligibility predicate.
// eligibility must come from account/rules policy; offline availability is an adapter policy.
std::array<bool, 4> difficultyMenuEnabled(int32_t initial,
    const std::array<uint8_t, 4> &suffix, const std::array<bool, 4> &eligibility);
} // namespace rb
