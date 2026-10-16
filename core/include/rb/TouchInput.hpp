#pragma once
#include "Touch.hpp"
namespace rb {
struct TouchSlotHistory {
    TouchPoint initial, previous;
    int32_t heldFrames = 0, marker = 1, stationaryFrames = 0, age = 0;
    bool moved = false, updated = false;
};
struct TouchInputConfig {
    int32_t matchDistance = 2720;              // original 0x10372084
    int32_t blockTop = 80, blockHeight = 1142; // supplied original rectangle globals
};
// Restored post-hardware point association. UIKit snapshots are already inverse
// mapped to original coordinates; system touch identities are not original IDs.
class TouchInput {
  public:
    TouchState state;
    std::array<TouchSlotHistory, 15> history{};
    int32_t nextId = 0;
    TouchInput();
    void reset(); // original 0x10060E50 pool fields; preserves nextId like pool reset
    void advance(const std::vector<TouchPoint> &, const TouchInputConfig & = {});
};
} // namespace rb
