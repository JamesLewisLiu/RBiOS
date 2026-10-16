#include "rb/Score.hpp"
#include "rb/Binary.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace rb {
namespace {
int32_t wrap(uint32_t n) {
    int32_t r;
    std::memcpy(&r, &n, 4);
    return r;
}
int32_t add(int32_t a, int32_t b) {
    return wrap(uint32_t(a) + uint32_t(b));
}
int32_t sub(int32_t a, int32_t b) {
    return wrap(uint32_t(a) - uint32_t(b));
}
int32_t mul(int32_t a, int32_t b) {
    return wrap(uint32_t(a) * uint32_t(b));
}
int32_t total(const PlayerScore &s) {
    int32_t n = 0;
    for (auto c : s.counts)
        n = add(n, c);
    return n;
}
float ratio(int32_t a, int32_t b) {
    if (!b)
        throw Error("invalid original score denominator");
    return float(double(a) / double(b));
}
int32_t truncate(double n) {
    if (!std::isfinite(n) || n < -2147483648.0 || n >= 2147483648.0)
        throw Error("score conversion outside int32");
    return int32_t(n);
}
// 0x100046D0 clamps after the original float store, upper bound first.
void clamp(float &n, float minimum, float maximum) {
    if (n >= maximum)
        n = maximum;
    if (n <= minimum)
        n = minimum;
}
} // namespace
void PlayerScore::reset() {
    counts.fill(0);
    timingHistogram.fill(0);
    extraCount = usedJustReflec = combo = maxCombo = displayedJustReflec = justReflec = 0;
    streak = initialStreak;
    fullCombo = true;
    displayedRate = overriddenRate = targetRate = 0;
    animationState = 0;
    overrideRate = retired = false;
}
void PlayerScore::consumeJustReflec() {
    // The x86 conditional subtraction retains the old value on signed overflow.
    int64_t n = int64_t(justReflec) - 100;
    if (n >= std::numeric_limits<int32_t>::min())
        justReflec = int32_t(n);
    justReflec = std::max(justReflec, 0);
    usedJustReflec = add(usedJustReflec, 1);
}
float PlayerScore::projectedRate() const {
    int32_t base = sub(totalEvents, extraEvents);
    if (add(extraEvents, base) <= 0)
        return 0;
    int32_t remaining = sub(add(add(extraCount, base), counts[0]), total(*this));
    int32_t middle = add(add(remaining, counts[2]), add(mul(2, remaining), mul(2, counts[1])));
    return ratio(sub(add(add(extraEvents, counts[3]), mul(2, middle)), extraCount),
                 add(extraEvents, mul(6, base)));
}
float PlayerScore::currentRate() const {
    int32_t base = sub(total(*this), extraCount);
    if (add(base, extraCount) <= 0)
        return 0;
    return ratio(add(counts[3], mul(2, add(add(counts[0], counts[2]),
                                           add(mul(2, counts[0]), mul(2, counts[1]))))),
                 add(extraCount, mul(6, base)));
}
float PlayerScore::finalRate() const {
    if (overrideRate)
        return overriddenRate;
    if (totalEvents <= 0)
        return 0;
    return ratio(add(counts[3], mul(2, add(counts[2], add(mul(3, counts[0]), mul(2, counts[1]))))),
                 add(extraEvents, mul(6, sub(totalEvents, extraEvents))));
}
int32_t PlayerScore::performanceScore(unsigned difficulty) const {
    constexpr float thresholds[] = {55, 65, 75, 75};
    if (difficulty >= 4)
        throw Error("invalid original difficulty");
    double threshold = thresholds[difficulty];
    float percent = float(double(finalRate()) * 100.0);
    float excess = float(std::max(double(percent) - threshold, 0.0));
    return truncate(double(excess) * 3000.0 / (100.0 - threshold) +
                    std::min(double(percent), threshold) * 7000.0 / threshold);
}
int32_t PlayerScore::clearType(unsigned difficulty, int32_t option) const {
    if (retired)
        return 1;
    if (option == 1 || option == 2)
        return 4;
    bool passed = performanceScore(difficulty) >= 7000;
    if (option != 4)
        return passed ? 3 : 2;
    return passed || counts[4] != 0 ? 3 : 2;
}
int32_t PlayerScore::medal(unsigned difficulty, int32_t option) const {
    if (clearType(difficulty, option) < 3 || !fullCombo || total(*this) != totalEvents)
        return 0;
    int32_t result = totalJustReflec && usedJustReflec >= totalJustReflec ? 3 : 1;
    return counts[1] == 0 && counts[2] == 0 ? result | 0x10 : result;
}
void BattleScore::reset() {
    *this = BattleScore{};
}
void BattleScore::adjustGauge(unsigned index, int32_t amount, bool stopAtOne) {
    if (index >= 5)
        throw Error("invalid gauge index");
    float &g = gauge[index];
    if (g <= 0 && amount < 0)
        return;
    float delta = float(amount);
    if (delta > 0) {
        if (index == 2)
            delta = float(double(delta) * scale[0]);
        if (index == 3)
            delta = float(double(delta) * scale[2]);
        if (index == 4)
            delta = float(double(delta) * scale[3]);
    }
    if (!stopAtOne || g > 0) {
        g = float(double(g) - delta);
        clamp(g, stopAtOne ? 1.f : 0.f, maxGauge[index]);
    }
}
void BattleScore::applyEffect(unsigned index, unsigned side, int32_t reason,
                              int32_t previousMaxCombo, bool stageModeSix) {
    if (side >= 2)
        throw Error("invalid score side");
    if (index > 7)
        return;
    constexpr int32_t points[] = {6, 4, 2, 1, 0, 10, 0, 50}; // 0x10284578
    int32_t amount = points[index];
    if (index == 7) {
        int32_t n = add(baseCount[side], extraCount[side]);
        amount = n > 0 ? std::min(50, mul(50, previousMaxCombo) / n) : 0;
    }
    if (amount)
        score[side] = std::clamp(add(score[side], amount), 0, 9999);
    auto change = [&](unsigned i, float delta, bool aliveOnly = false, float minimum = 0) {
        if (!aliveOnly || gauge[i] > 0) {
            gauge[i] = float(double(gauge[i]) - delta);
            clamp(gauge[i], minimum, maxGauge[i]);
        }
    };
    bool protect = reason == 4 || reason == 6;
    if (side) {
        if (index == 6 && !(ruleFlags & 2))
            change(4, float(double(scale[3]) * 10.0));
        return;
    }
    if (index == 7 && !stageModeSix)
        animateScore = {{true, true}};
    switch (index) {
    case 0:
        if (recoveryBonus)
            adjustGauge(2, wrap(0u - uint32_t(recoveryBonus)), false);
        change(0, -9, true);
        change(1, -9, true);
        if (!(ruleFlags & 0x10))
            change(3, -1, true);
        if (!(ruleFlags & 2))
            change(4, float(double(scale[3]) + scale[3]));
        break;
    case 1:
        if (!(ruleFlags & 2))
            change(4, scale[3]);
        break;
    case 2:
        adjustGauge(2, 100, false);
        change(0, 400);
        change(1, 1000);
        change(3, float(double(scale[2]) + scale[2]));
        break;
    case 4:
        adjustGauge(2, 250, protect);
        change(0, 1500, protect, protect ? 1.f : 0.f);
        change(1, 3000, protect, protect ? 1.f : 0.f);
        change(3, float(double(scale[2]) * 5.0), protect, protect ? 1.f : 0.f);
        break;
    case 5:
        change(2, -125, true);
        change(0, -190, true);
        change(1, -190, true);
        if (!(ruleFlags & 0x10))
            change(3, -5, true);
        // Original case 5 reaches LABEL_62 regardless of ruleFlags bit 1.
        change(4, float(double(scale[3]) + scale[3]));
        break;
    case 6:
        change(0, 1500);
        change(3, float(double(scale[2]) * (ruleFlags & 1 ? 50.0 : 10.0)));
        break;
    default:
        break;
    }
}
void BattleScore::updateLife(unsigned side, unsigned grade) {
    if (side >= 2 || grade >= 5)
        throw Error("invalid life event");
    if (life[side] > 0)
        life[side] = add(life[side], lifeDelta[grade]);
    life[side] = std::max(std::min(life[side], 5000), lifeFloor);
}
void submitScore(BattleScore &b, PlayerScore &p, const ScoreEvent &e, bool updateRemoteCombo,
                 bool stageModeSix) {
    if (e.side >= 2 || e.grade >= 5)
        throw Error("invalid judgment event");
    int32_t oldMax = p.maxCombo;
    p.counts[e.grade] = add(p.counts[e.grade], 1);
    if (e.reason == 4 || e.reason == 6)
        p.extraCount = add(p.extraCount, 1);
    if (!e.side || updateRemoteCombo) {
        if (e.grade == 4) {
            p.fullCombo = false;
            p.combo = p.streak = 0;
        } else {
            p.combo = add(p.combo, 1);
            p.streak = add(p.streak, 1);
            p.maxCombo = std::max(p.maxCombo, p.combo);
        }
    }
    unsigned bucket = e.grade == 4 ? 0 : unsigned(std::clamp(e.deviation, -3, 3) + 4);
    p.timingHistogram[bucket] = add(p.timingHistogram[bucket], 1);
    if (!(e.grade == 4 && (e.reason == 4 || e.kind == 1))) {
        constexpr int32_t increments[] = {5, 3, 1, 0, 5, 4, 2, 1,
                                          0, 4, 3, 2, 1, 0, 3}; // 0x10292674
        unsigned tier = b.baseCount[e.side] <= 200 ? 0 : b.baseCount[e.side] <= 400 ? 1 : 2;
        p.justReflec = std::clamp(add(p.justReflec, increments[tier * 5 + e.grade]), 0, 500);
    }
    if (e.specialNote && e.grade == 4 && e.specialPenalty)
        b.applyEffect(6, e.side, e.reason, oldMax, stageModeSix);
    b.applyEffect(e.grade, e.side, e.reason, oldMax, stageModeSix);
    b.updateLife(e.side, e.grade);
}
int32_t lifeIncrement(int32_t level, unsigned grade, int32_t totalNotes) {
    constexpr float values[] = {16, 16, 8, 1, -12};
    if (grade >= 5 || totalNotes < 0 || totalNotes >= 2147483647)
        throw Error("invalid life configuration");
    level = std::min(level, 10);
    if (grade > 2)
        return truncate(double(values[grade]) * 5000.0 / 300.0);
    int32_t n = totalNotes + 1;
    if (n >= 350)
        n = (totalNotes - 349) / 3 + 350;
    float v = values[grade];
    if (grade == 1) {
        float l = float(level);
        v = float(double(v) - 11.0 * l * l / 100.0);
        v = std::min(v, float(16.0 - double(l) * 8.0 / 10.0));
    } else
        v = float(double(v) - double(level) * 8.0 / 10.0);
    return truncate(double(std::max(v, 0.f)) * 5000.0 / (double(n) * 6.0));
}
} // namespace rb
