#pragma once
#include "Judgment.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace rb {
struct JudgmentResult {
    int32_t grade = 6, reason = -1;
};
// Selected CNoteBase fields used by the dispatcher. Platform/CPU/touch objects
// remain typed inputs; no original vtable or x86 pointer is copied to ARM64.
struct DispatchNote : NoteTiming {
    bool completed = false, held = false;         // +308/+309
    int32_t style = 0, contactId = -1;            // +312/+216
    int32_t switchCount = 0, switchDirection = 0; // chart +396 / note +372
    bool touchReleased = false;                   // +409
    int8_t replayCursor = -1;                     // +417
};
struct JudgmentQueries {
    std::function<bool()> missed;                        // note virtual +188 / 0x10158550
    std::function<bool()> pressed;                       // note virtual +184 / 0x10159820
    std::function<bool()> holdCovered;                   // 0x10158BA0
    std::function<std::optional<int32_t>()> switchPress; // 0x10159610; reason 5/7
    std::function<bool()> switchHeld;                    // 0x10159D10 with note +372
    std::function<std::optional<JudgmentResult>()> cpu;  // 0x1015DBC0, absent = no CPU object
    // Original replay lookup 0x10090340, preselected by chart object ID (+8).
    // nullptr means no replay object; an empty vector means a present, empty record.
    const std::vector<uint8_t> *replay = nullptr;
};
// 0x1016B940 and its 0x1016AC80..1016B740 helpers. Mode is the original numeric
// stage mode, distinct from the timing-table selector. Queries are lazy because
// original hit tests mutate touch ownership/grace and must not run speculatively.
std::optional<JudgmentResult> dispatchJudgment(DispatchNote &, int32_t mode, uint32_t timingMode,
                                               bool rejectOuter, const JudgmentQueries &);
} // namespace rb
