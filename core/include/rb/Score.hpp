#pragma once
#include <array>
#include <cstdint>

namespace rb {
// Offsets in comments identify x86 evidence, not a memory layout to copy to ARM64.
struct PlayerScore {
    std::array<int32_t, 5> counts{};            // +0; original numeric judgment codes 0..4
    int32_t extraCount = 0, usedJustReflec = 0; // +20, +24
    std::array<int32_t, 8> timingHistogram{};   // +28; miss, then -3..+3
    int32_t combo = 0, maxCombo = 0, streak = 0, initialStreak = 0; // +60..72
    int32_t side = 0, chartSide = 0, totalEvents = 0, extraEvents = 0, totalJustReflec = 0;
    int32_t displayedJustReflec = 0, justReflec = 0; // +96, +100
    bool fullCombo = true;                           // +104
    float displayedRate = 0;                         // +108
    int32_t animationState = 0;                      // +112
    bool overrideRate = false;                       // +116
    float overriddenRate = 0, targetRate = 0;        // +120, +124
    bool retired = false;                            // +128

    void reset();                // 0x10164160; preserves chart metadata and initialStreak
    void consumeJustReflec();    // 0x10164260
    float projectedRate() const; // 0x101642B0
    float currentRate() const;   // 0x10164340
    float finalRate() const;     // 0x101643B0
    int32_t performanceScore(unsigned difficulty) const;          // 0x10164450
    int32_t clearType(unsigned difficulty, int32_t option) const; // 0x10164540
    int32_t medal(unsigned difficulty, int32_t option) const;     // 0x101647C0
};

struct BattleScore {
    std::array<int32_t, 2> score{}, displayedScore{};               // +0, +8
    std::array<bool, 2> animateScore{};                             // +16
    std::array<float, 5> gauge{}, displayedGauge{}, maxGauge{};     // +20, +40, +60
    std::array<float, 4> scale{{0, 0, 1, 1}};                       // +80
    int32_t recoveryBonus = 0;                                      // +96
    std::array<int32_t, 2> baseCount{}, extraCount{}, parameters{}; // +100, +108, +116
    bool stateFlag = false;                                         // +124
    std::array<int32_t, 2> aggregates{}, life{};                    // +128, +136
    std::array<int32_t, 5> lifeDelta{};                             // +144
    int32_t lifeFloor = 0;                                          // +164
    uint32_t ruleFlags = 0;                                         // +168

    // No invented stage defaults: the caller must supply the recovered chart/config values.
    void reset();                                                     // 0x101548F0
    void adjustGauge(unsigned index, int32_t amount, bool stopAtOne); // 0x10154C80
    void applyEffect(unsigned index, unsigned side, int32_t reason, int32_t previousMaxCombo,
                     bool stageModeSix);            // 0x10154D40
    void updateLife(unsigned side, unsigned grade); // 0x10154660
};
struct ScoreEvent {
    unsigned side = 0, grade = 0;
    int32_t reason = 0, kind = 0, deviation = 0;
    bool specialNote = false;    // result of original note virtual +180; not inferred here
    bool specialPenalty = false; // note +426
};
// Replaces reads of the original remote-session/stage globals with explicit inputs.
void submitScore(BattleScore &, PlayerScore &, const ScoreEvent &, bool updateRemoteCombo,
                 bool stageModeSix);                                      // 0x10155260
int32_t lifeIncrement(int32_t level, unsigned grade, int32_t totalNotes); // 0x101546C0
} // namespace rb
