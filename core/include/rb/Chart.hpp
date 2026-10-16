#pragma once
#include "Binary.hpp"
#include <array>

namespace rb {
// Restored wire fields from 0x1014B870. These are NOT an ARM64 memory overlay.
struct ChartObject {
    int32_t startMs = 0, travelMs = 0;
    int16_t id = 0, group = 0;
    std::vector<int16_t> links;
    std::array<uint8_t, 4> attributes{};
    std::array<int16_t, 4> parameters{};
    uint32_t flags = 0;
    std::array<uint8_t, 2> tailBytes{};
    int16_t tailWord = 0;
    int32_t tailValue = 0;
    std::array<int32_t, 3> extension{};
    int32_t startTick() const;
    int32_t travelTicks() const;
    int32_t durationTicks() const;
    int destination() const;
};
struct ChartCommand {
    int16_t type = 0, reserved = 0;
    int32_t timeMs = 0;
    std::array<int32_t, 7> fields{};
};
struct ChartExtra {
    int16_t object = 0, type = 0;
    int32_t timeMs = 0, durationMs = 0, value = 0;
};
struct Chart {
    uint32_t version = 0;
    float bpm = 0;
    int32_t durationMs = 0, unknownTime = 0;
    uint16_t declaredEvents = 0, unknownCount = 0;
    uint32_t unknownHeader = 0;
    std::vector<ChartObject> objects;
    std::vector<ChartCommand> commands;
    std::vector<ChartExtra> extras;
    static Chart parse(const Bytes &);
    // 0x10149AA0 + 0x101497D0: additional long/switch ticks, NOT final score.
    int32_t additionalTicks(size_t object) const;
    std::vector<int32_t> switchDurations(size_t object) const; // 0x101497D0
};
int32_t millisecondsToTicks(int32_t);         // double at 0x10370730, 60.0
std::array<uint32_t, 4> randomSeed(uint32_t); // 0x10065D20
} // namespace rb
