#include "rb/StageScoreInitialization.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace rb {
namespace {
int32_t wrappedProduct(int32_t a, int32_t b) {
    const uint32_t n = uint32_t(a) * uint32_t(b);
    int32_t result; std::memcpy(&result, &n, 4); return result;
}
int32_t wrappedSubtract(int32_t a, int32_t b) {
    const uint32_t n = uint32_t(a) - uint32_t(b);
    int32_t result; std::memcpy(&result, &n, 4); return result;
}
}
StageScoreInitialization StageScoreInitialization::offline(const MusicSheet &s, uint8_t level) {
    StageScoreInitialization out;
    out.total = s.counts.total;
    out.longTicks = s.counts.longTicks; // original sheet +120, not extras +104
    out.afterJustReflec = s.counts.afterJustReflec;
    out.selectedChartSide = unsigned(s.options.localChartSide);
    out.lifeLevel = level >= 1 && level <= 15 ? int32_t(level) - 1 : -1;
    return out;
}
BattleScore initializeStageScore(const StageScoreInitialization &in) {
    if (in.selectedChartSide > 1) throw Error("invalid stage chart side");
    BattleScore out;
    out.scale = in.scale;
    out.ruleFlags = in.ruleFlags;
    out.lifeFloor = in.removeLifeFloor ? 0 : 100;
    out.life = {100,100};
    for (unsigned side = 0; side < 2; ++side) {
        auto chartSide = side == 0 ? in.selectedChartSide : unsigned(in.selectedChartSide == 0);
        out.extraCount[side] = in.longTicks[chartSide];
        auto base = out.baseCount[side] = wrappedSubtract(in.total[chartSide],out.extraCount[side]);
        auto multiplier = base <= 200 ? 5 : base <= 400 ? 4 : 3; // 10292674
        out.aggregates[side] = wrappedProduct(multiplier,wrappedSubtract(base,in.afterJustReflec[chartSide]));
        out.parameters[side] = out.aggregates[side] / 100;
    }
    for (unsigned grade = 0; grade < 5; ++grade)
        out.lifeDelta[grade] = lifeIncrement(in.lifeLevel,grade,in.total[in.selectedChartSide]);
    for (unsigned i = 0; i < 5; ++i) {
        out.gauge[i] = out.displayedGauge[i] = float(in.gauge[i]);
        out.maxGauge[i] = float(in.maximum[i]);
    }
    if (out.baseCount[0] <= 0) throw Error("stage score requires positive original base count");
    const auto n = double(std::max(out.parameters[0],1))*out.maxGauge[2]*3.75 / (double(out.baseCount[0])*100.0);
    if (!std::isfinite(n) || n < INT32_MIN || n >= 2147483648.0) throw Error("stage recovery bonus outside int32");
    out.recoveryBonus = int32_t(n);
    return out;
}
} // namespace rb
