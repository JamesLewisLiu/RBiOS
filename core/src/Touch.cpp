#include "rb/Touch.hpp"
#include "rb/Binary.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
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
bool inside(int32_t x, int32_t y, const TouchRect &r) {
    return r.x < x && x < add(r.x, r.width) && r.y < y && y < add(r.y, r.height);
}
float scale(float s) {
    if (!std::isfinite(s) || s < 0 || s > 100)
        throw Error("invalid original touch scale");
    return std::min(s, 1.f);
}
TouchRect rectangle(const NoteTouchState &n, float s, bool reverse) {
    s = scale(s);
    int32_t dx = int32_t(double(s) * (reverse && !n.wide ? 72.0 : 144.0));
    if (!n.wide)
        s = std::min(s, .9f);
    int32_t dy = int32_t(double(s) * 108.0);
    return {n.x - dx, n.y - dy, 2 * dx, n.region != 1 && n.y < 976 ? 0 : 2 * dy};
}
} // namespace
bool fieldContains(const FieldGeometry &field, int32_t direction, int32_t y, int32_t radius,
                   int32_t margin, int32_t x) {
    switch (direction) {
    case 0:
        return sub(x, radius) >= sub(field.x(0), margin);
    case 1:
        return add(radius, x) <= add(margin, field.x(1));
    case 2:
        return sub(y, radius) >= sub(field.y(1), margin);
    case 3:
        return add(radius, y) <= add(margin, field.y(0));
    default:
        return false;
    }
}
bool NoteTouchState::inField(const FieldGeometry &field, int32_t direction, int32_t margin,
                             float s) const {
    scale(s); // finite/range guard only; original radius uses the unclamped scale.
    return fieldContains(field, direction, y, int32_t(double(s) * 36.0), margin, x);
}
bool TouchState::held(const TouchRect &r) {
    for (auto &c : contacts)
        if (c.active && (c.stateBits & 2) && !c.blocked && inside(c.x, c.y, r)) {
            lastSelectedId = c.id;
            return true;
        }
    return false;
}
bool TouchState::supplementary(const TouchRect &r) const {
    for (auto &p : supplemental)
        if (inside(p.x, p.y, r))
            return true;
    return false;
}
TouchContact *TouchState::find(int32_t id) {
    for (auto &c : contacts)
        if (c.active && c.id == id)
            return &c;
    return nullptr;
}
bool TouchState::press(const TouchRect &r, bool allowSelected, bool capture, TouchPoint *out) {
    float closest = 9999.f;
    TouchContact *chosen = nullptr;
    int32_t cx = add(add(r.x, r.width), r.x) / 2, cy = add(r.y, add(r.y, r.height)) / 2;
    for (auto &c : contacts)
        if (c.active && (c.stateBits & 1) && !c.blocked && (!c.selected || allowSelected) &&
            !c.captured && inside(c.x, c.y, r)) {
            float dx = float(std::abs(int64_t(c.x) - cx)), dy = float(std::abs(int64_t(c.y) - cy));
            float square = float(double(dx) * dx + double(dy) * dy);
            float distance = float(std::sqrt(double(square)));
            if (closest > distance) {
                closest = distance;
                chosen = &c;
            }
        }
    if (!chosen)
        return false;
    if (out)
        *out = {chosen->x, chosen->y};
    lastSelectedId = chosen->id;
    chosen->selected = true;
    chosen->captured = capture;
    return true;
}
bool NoteTouchState::extendedWindow() const {
    return wide && kind != 4 && boundaryY <= y && y < (region == 1 ? extendedBottom : fieldBottom);
}
bool NoteTouchState::touchWindow() const {
    if (ended || cooldown > 0)
        return false;
    int32_t first = kind == 4   ? sub(switchWindowStart, durationTicks)
                    : kind == 1 ? wrap(0u - uint32_t(durationTicks))
                                : windowStart;
    return (first <= relativeTick && relativeTick <= windowEnd) || extendedWindow();
}
bool NoteTouchState::maintainHold(TouchState &t, float s, bool supplemental) {
    if (phase <= 2)
        return true;
    auto r = rectangle(*this, s, false);
    if (t.held(r) || (supplemental && t.supplementary(r))) {
        grace = 0;
        return true;
    }
    grace = add(grace, 1);
    return grace <= 5;
}
bool NoteTouchState::maintainSwitch(TouchState &t, float s, bool supplemental) {
    if (!segmentReverse)
        return maintainHold(t, s, supplemental);
    return phase <= 2 || !t.held(rectangle(*this, s, true));
}
bool NoteTouchState::switchPress(TouchState &t, float s, int32_t &reason) {
    if (side || inputBlocked || !touchWindow() || kind != 4)
        return false;
    auto r = rectangle(*this, s, false);
    int32_t remaining = segmentIndex < 0 ? relativeTick : sub(segmentDuration, segmentElapsed);
    bool boundary = remaining >= -9 && remaining <= 9;
    if (boundary) {
        if (segmentReverse) {
            if (t.press(r, false, false, &lastPress)) {
                touchId = t.lastSelectedId;
                reason = 5;
                return true;
            }
        } else if (touchId != -1 && !t.held(r)) {
            reason = 5;
            touchId = -1;
            return true;
        }
    } else if (!segmentReverse && touchId == -1 && t.press(r, false, false, &lastPress)) {
        touchId = t.lastSelectedId;
        reason = 7;
        return true;
    }
    return false;
}
bool NoteTouchState::firstPress(TouchState &t, float s) {
    if (side || inputBlocked || !touchWindow())
        return false;
    s = scale(s);
    int32_t dx = widthMode == 0 ? 180
                 : widthMode == 1
                     ? 120
                     : int32_t(double(!wide && kind != 1 ? std::min(s, .9f) : s) * 144.0);
    if (!wide)
        s = std::min(s, .9f);
    int32_t dy = int32_t(double(s) * 108.0), top = y - dy, height = 2 * dy;
    if (region != 1 && boundaryY < y - dy) {
        int32_t h = dy + y - boundaryY;
        top += height / 2 - h;
        height = 2 * h;
    }
    if (region != 1 && y < 976)
        height = 0;
    if (!t.press({x - dx, top, 2 * dx, height}, false, false, &lastPress))
        return false;
    touchId = t.lastSelectedId;
    if (kind != 1 && kind != 4 && initialTiming <= 0)
        t.find(touchId)->reuseDelay = 40.f;
    return true;
}
bool NoteTouchState::missed(float s, bool remoteWide,
                            const std::function<bool(int32_t, int32_t)> &geometry) const {
    if (relativeTick > 0)
        return false;
    if (relativeTick <= (kind == 1 || kind == 4 ? -1000 : -500))
        return true;
    if (kind == 1)
        return noteMode == 6 && relativeTick < -9;
    if (kind == 4)
        return (segmentIndex < 0 ? relativeTick : sub(segmentDuration, segmentElapsed)) < -9;
    if (region == 1)
        return missFlag;
    scale(s);
    if (!geometry)
        throw Error("original note +172 geometry unavailable");
    int32_t margin = int32_t(double(s) * 36.0 * (side == 1 && remoteWide ? 1.0 : .5));
    return !geometry(side ? 2 : 3, margin);
}
} // namespace rb
