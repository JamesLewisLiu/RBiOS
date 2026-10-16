#pragma once
#include "rb/Judgment.hpp"
#include "rb/Motion.hpp"
#include <array>
#include <functional>
#include <vector>
namespace rb {
bool fieldContains(const FieldGeometry &, int32_t direction, int32_t y, int32_t radius,
                   int32_t margin, int32_t x); // 0x100A4F80
struct TouchPoint {
    int32_t x = 0, y = 0;
};
struct TouchRect {
    int32_t x = 0, y = 0, width = 0, height = 0;
};
struct TouchContact {
    bool active = false;
    int32_t id = 0, x = 0, y = 0;
    uint8_t stateBits = 0;
    bool selected = false, captured = false, blocked = false;
    float reuseDelay = 0; // +60; first-press writes 40 for ordinary notes
};
struct TouchState {
    std::array<TouchContact, 15> contacts{};
    std::vector<TouchPoint> supplemental;
    int32_t lastSelectedId = 0;
    bool held(const TouchRect &);                // 0x10061590 / 10061630
    bool supplementary(const TouchRect &) const; // 0x10061520
    TouchContact *find(int32_t);                 // 0x10061700
    bool press(const TouchRect &, bool allowSelected = false, bool capture = false,
               TouchPoint *output = nullptr); // 0x10061CB0 / 10061DF0
};
struct NoteTouchState : NoteTiming {
    int32_t x = 0, y = 0, phase = 0, grace = 0, side = 0, region = 0;
    int32_t touchId = -1, segmentReverse = 0, cooldown = 0;
    int32_t windowStart = 0, windowEnd = 0, switchWindowStart = 0;
    int32_t boundaryY = 0, extendedBottom = 0, initialTiming = 0, widthMode = 0, noteMode = 0;
    int32_t fieldBottom = 1222; // 0x100A4B90(0), original default field; explicit runtime input
    bool wide = false, inputBlocked = false, missFlag = false;
    TouchPoint lastPress{};
    bool extendedWindow() const; // 0x101586B0
    bool touchWindow() const;    // 0x10158750; invokes original +168 condition
    bool maintainHold(TouchState &, float touchScale, bool useSupplemental);   // 0x10158BA0
    bool maintainSwitch(TouchState &, float touchScale, bool useSupplemental); // 0x10159D10
    bool switchPress(TouchState &, float touchScale, int32_t &reason);         // 0x10159610
    bool firstPress(TouchState &, float touchScale);                           // 0x10159820
    bool inField(const FieldGeometry &, int32_t direction, int32_t margin,
                 float touchScale) const; // virtual +172, 0x101584B0
    // Caller can bind inField with the current original field and touch scale.
    bool missed(float touchScale, bool remoteWide,
                const std::function<bool(int32_t, int32_t)> &geometry) const; // 0x10158550
};
} // namespace rb
