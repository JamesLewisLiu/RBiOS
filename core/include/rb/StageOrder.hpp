#pragma once
#include <cstdint>
#include <vector>
namespace rb {
struct StageOrderEntry {
    uint32_t identity = 0;
    bool active = false;
    int32_t relativeTick = 0;
};
// 10157900 plus the shipped CRT 100302D0/10030240. Equal active ticks
// compare as -1 in the original, so a host std::sort cannot reproduce ties.
void sortStageOrder(std::vector<StageOrderEntry> &);
} // namespace rb
