#pragma once
#include "MusicSheet.hpp"
#include "Score.hpp"

namespace rb {
struct StageScoreInitialization {
    std::array<int32_t, 2> total{}, longTicks{}, afterJustReflec{};
    unsigned selectedChartSide = 0;
    uint32_t ruleFlags = 0;
    bool removeLifeFloor = false;
    int32_t lifeLevel = -1; // catalog signed level -1, optionally profile override
    std::array<int32_t, 5> gauge{{10000,10000,0,0,0}};
    std::array<int32_t, 5> maximum{{10000,10000,0,0,0}};
    std::array<float, 4> scale{{1,1,1,1}};
    // Default configuration values restored from 0x1007FFD0; no saved profile.
    static StageScoreInitialization offline(const MusicSheet &, uint8_t catalogLevel);
};
BattleScore initializeStageScore(const StageScoreInitialization &); // 0x101554C0
} // namespace rb
