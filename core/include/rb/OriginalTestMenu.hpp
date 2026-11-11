#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace rb {
struct OriginalTestEntry { int32_t scene; std::string label; uint32_t flags = 0; };
enum class OriginalTestAction { none, enterScene, exit };
struct OriginalTestDecision {
    OriginalTestAction action = OriginalTestAction::none;
    int32_t scene = 0;
    bool requestEventMode = false;
};
// Original 100D5AF0 tables. Region 0 uses 103671E8, regions 1..3 use 103672D8.
std::vector<OriginalTestEntry> originalTestEntries(unsigned region);
// 100D5780 changes bit 1 only; inputs are original external state, not defaults.
void updateOriginalTestExitFlags(std::vector<OriginalTestEntry> &, bool blocked, uint16_t mode);
// Original 100D5600. Caller performs the returned scene/exit dispatch. No side
// effects are faked and a decision does not mean the target scene is migrated.
OriginalTestDecision updateOriginalTestMenu(const std::vector<OriginalTestEntry> &,
    int32_t &selected, uint32_t pressed, uint32_t repeated);
} // namespace rb
