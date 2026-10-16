#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rb {
// Original shared CRandom state, now explicit so a replay supplies the same
// stream to every consumer. Do not reseed it separately for each note.
struct OriginalRandom {
    std::array<uint32_t, 4> words;
    static OriginalRandom seeded(uint32_t seed); // 0x10065D20
    uint32_t next();
    float range(float lower, float upper); // 0x100658E0
    bool chance(float percent);            // 0x100659D0
};
struct CPUParameters {
    float ratioLower, ratioUpper, specialPercent;
};
CPUParameters cpuParameters(uint32_t difficulty); // 0x1036C808, 19 recovered rows

struct CPUEntry {
    int32_t judgment = 6;               // original +52 array
    int32_t tick = 0;                   // original +56 array
    uint8_t consumed = 0, reserved = 0; // original +60/+64 arrays
};
struct CPUNoteInput {
    int32_t index = -1;       // original chart object +324
    int32_t kind = 0;         // original chart object +288
    uint32_t flags = 0;       // original chart object +300
    int32_t relativeTick = 0; // original note +192
    int32_t segmentIndex = -1, segmentDuration = 0, segmentElapsed = 0;
};
struct CPUJudgment {
    int32_t judgment, event; // original two out-parameters; event 11 or 12
};
class CPUState {
  public:
    std::vector<CPUEntry> entries;
    int32_t lastIndex = -1;       // original +20 (not reset by 0x1015DE30)
    bool specialEnabled = true;   // original +24, constructor 0x1015D8E0
    int32_t specialOverride = -1; // original +68
    int32_t meter = 0;            // original *cpu[1] +100
    uint32_t difficulty = 0;      // original *cpu[3] +20

    // Caller resolves chart count and difficulty using the original stage data.
    // Restores 0x1015DE30: distribution, original seeded shuffle, tick offsets.
    void initialize(uint32_t noteCount, uint32_t level, OriginalRandom &random);
    static std::array<int32_t, 5> distribution(uint32_t noteCount, uint32_t level,
                                               OriginalRandom &random);          // 0x1015DA20
    void assignOffsets(OriginalRandom &random);                                  // 0x1015D990
    int32_t reserveNext(int32_t judgment);                                       // 0x1015D800
    std::optional<CPUJudgment> evaluate(const CPUNoteInput &, OriginalRandom &); // 0x1015DBC0
};
} // namespace rb
