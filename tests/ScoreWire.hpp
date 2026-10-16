#pragma once
#include "rb/Binary.hpp"
#include "rb/Score.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
using namespace rb;
namespace {
// Test-only wire image for differential comparison with x86 memory; never used by the game.
std::vector<uint8_t> wire(304);
template <class T> void field(T &v, unsigned offset, bool read) {
    static_assert(sizeof(T) == 4);
    if (read)
        std::memcpy(&v, wire.data() + offset, 4);
    else
        std::memcpy(wire.data() + offset, &v, 4);
}
void flag(bool &v, unsigned offset, bool read) {
    if (read)
        v = wire[offset] != 0;
    else
        wire[offset] = uint8_t(v);
}
void transfer(BattleScore &b, PlayerScore &p, bool read) {
    for (unsigned i = 0; i < 2; ++i) {
        field(b.score[i], 4 * i, read);
        field(b.displayedScore[i], 8 + 4 * i, read);
        flag(b.animateScore[i], 16 + i, read);
        field(b.baseCount[i], 100 + 4 * i, read);
        field(b.extraCount[i], 108 + 4 * i, read);
        field(b.parameters[i], 116 + 4 * i, read);
        field(b.aggregates[i], 128 + 4 * i, read);
        field(b.life[i], 136 + 4 * i, read);
    }
    for (unsigned i = 0; i < 5; ++i) {
        field(b.gauge[i], 20 + 4 * i, read);
        field(b.displayedGauge[i], 40 + 4 * i, read);
        field(b.maxGauge[i], 60 + 4 * i, read);
        field(b.lifeDelta[i], 144 + 4 * i, read);
        field(p.counts[i], 172 + 4 * i, read);
    }
    for (unsigned i = 0; i < 4; ++i)
        field(b.scale[i], 80 + 4 * i, read);
    field(b.recoveryBonus, 96, read);
    flag(b.stateFlag, 124, read);
    field(b.lifeFloor, 164, read);
    field(b.ruleFlags, 168, read);
    field(p.extraCount, 192, read);
    field(p.usedJustReflec, 196, read);
    for (unsigned i = 0; i < 8; ++i)
        field(p.timingHistogram[i], 200 + 4 * i, read);
    field(p.combo, 232, read);
    field(p.maxCombo, 236, read);
    field(p.streak, 240, read);
    field(p.initialStreak, 244, read);
    field(p.side, 248, read);
    field(p.chartSide, 252, read);
    field(p.totalEvents, 256, read);
    field(p.extraEvents, 260, read);
    field(p.totalJustReflec, 264, read);
    field(p.displayedJustReflec, 268, read);
    field(p.justReflec, 272, read);
    flag(p.fullCombo, 276, read);
    field(p.displayedRate, 280, read);
    field(p.animationState, 284, read);
    flag(p.overrideRate, 288, read);
    field(p.overriddenRate, 292, read);
    field(p.targetRate, 296, read);
    flag(p.retired, 300, read);
}
[[maybe_unused]] void readWire(BattleScore &b, PlayerScore &p) {
    std::string s;
    std::cin >> s;
    if (s.size() != 608)
        throw Error("bad score fixture");
    for (unsigned i = 0; i < 304; ++i)
        wire[i] = uint8_t(std::stoul(s.substr(2 * i, 2), nullptr, 16));
    transfer(b, p, true);
}
[[maybe_unused]] void writeWire(BattleScore &b, PlayerScore &p) {
    transfer(b, p, false);
    constexpr char h[] = "0123456789abcdef";
    for (auto v : wire)
        std::cout << h[v >> 4] << h[v & 15];
    std::cout << '\n';
}
[[maybe_unused]] uint32_t bits(float n) {
    uint32_t r;
    std::memcpy(&r, &n, 4);
    return r;
}
} // namespace
