#include "rb/TouchInput.hpp"
#include <cmath>
namespace rb {
namespace {
float distance(TouchPoint a, TouchPoint b) {
    const float dx = float(std::abs(int64_t(a.x) - b.x)), dy = float(std::abs(int64_t(a.y) - b.y));
    const float square = float(double(dx) * dx + double(dy) * dy);
    return float(std::sqrt(double(square)));
}
void clear(TouchContact &c, TouchSlotHistory &h) {
    c = {};
    c.id = -1;
    c.stateBits = 255;
    c.reuseDelay = -1;
    h = {};
}
bool duplicate(const TouchInput &in, TouchPoint p, const TouchInputConfig &config) {
    for (size_t i = 0; i < 15; ++i) {
        const auto &c = in.state.contacts[i];
        const auto &h = in.history[i];
        if (!c.active)
            continue;
        if (c.stateBits & 2) {
            if (h.heldFrames >= 2 && std::abs(int64_t(c.x) - p.x) < 40 &&
                (c.y >= int64_t(config.blockTop) + config.blockHeight || c.y <= config.blockTop))
                return true;
        } else if ((c.stateBits & 1) && h.initial.x == p.x &&
                   std::abs(int64_t(h.initial.y) - p.y) < 40)
            return true;
    }
    return false;
}
} // namespace
TouchInput::TouchInput() {
    reset();
}
void TouchInput::reset() {
    for (size_t i = 0; i < 15; ++i)
        clear(state.contacts[i], history[i]);
    state.supplemental.clear();
}
void TouchInput::advance(const std::vector<TouchPoint> &points, const TouchInputConfig &config) {
    if (points.size() > 25 || config.matchDistance < 0)
        throw Error("invalid original touch snapshot");
    // Bound only platform coordinates, preventing undefined int arithmetic in
    // malformed snapshots; original native logical iPad coordinates fit here.
    for (auto p : points)
        if (p.x < -4000 || p.x > 5000 || p.y < -4000 || p.y > 5000)
            throw Error("touch coordinate out of supported range");
    // 0x100619D0: clear transient flags; previous release slots become free now.
    for (size_t i = 0; i < 15; ++i)
        if (state.contacts[i].active) {
            auto &c = state.contacts[i];
            c.selected = false;
            c.captured = false;
            history[i].updated = false;
            if (c.stateBits & 4)
                clear(c, history[i]);
        }
    std::array<std::array<float, 25>, 15> distances{};
    std::array<bool, 15> candidates{};
    std::array<bool, 25> consumed{};
    // 0x10061E50: global closest pair, ties preserve slot then snapshot order.
    for (size_t i = 0; i < 15; ++i) {
        const auto &c = state.contacts[i];
        candidates[i] = c.active && (c.stateBits & 3) && !history[i].updated;
        for (size_t j = 0; j < points.size(); ++j)
            distances[i][j] = distance({c.x, c.y}, points[j]);
    }
    while (true) {
        float best = float(config.matchDistance);
        size_t slot = 15, point = 25;
        for (size_t i = 0; i < 15; ++i)
            if (candidates[i] && !history[i].updated)
                for (size_t j = 0; j < points.size(); ++j)
                    if (!consumed[j]) {
                        const float d = distances[i][j], limit = state.contacts[i].reuseDelay;
                        if (d >= 0 && best > double(d) && (limit < 0 || limit > double(d))) {
                            best = d;
                            slot = i;
                            point = j;
                        }
                    }
        if (slot == 15)
            break;
        auto &c = state.contacts[slot];
        auto &h = history[slot];
        const auto p = points[point];
        // 0x10060FD0, including permanent 20-pixel movement marker.
        if (c.x == p.x && c.y == p.y)
            ++h.stationaryFrames;
        else
            h.stationaryFrames = 0;
        h.previous = {c.x, c.y};
        c.x = p.x;
        c.y = p.y;
        c.active = true;
        const int64_t dx = int64_t(p.x) - h.initial.x, dy = int64_t(p.y) - h.initial.y;
        if (!h.moved && dx * dx + dy * dy >= 400)
            h.moved = true;
        ++h.heldFrames;
        ++h.age;
        h.marker = 1;
        h.updated = true;
        c.stateBits = 2;
        consumed[point] = true;
    }
    for (size_t i = 0; i < 15; ++i) {
        auto &c = state.contacts[i];
        auto &h = history[i];
        if (c.active && (c.stateBits & 3) && !h.updated) {
            ++h.age;
            c.active = true;
            c.stateBits = 4;
            h.updated = true;
        }
    }
    // 0x10061810: issue IDs even when the pool is exhausted, fill first free slot.
    size_t unconsumed = 0;
    for (size_t j = 0; j < points.size(); ++j)
        if (!consumed[j])
            ++unconsumed;
    for (size_t j = 0; j < points.size(); ++j)
        if (!consumed[j]) {
            TouchContact c;
            TouchSlotHistory h;
            auto p = points[j];
            c.active = true;
            c.id = nextId;
            c.x = p.x;
            c.y = p.y;
            c.stateBits = 1;
            c.reuseDelay = -1;
            h.initial = h.previous = p;
            h.age = 1;
            h.updated = true;
            nextId = nextId == INT32_MAX ? 0 : nextId + 1;
            if (unconsumed >= 3 && duplicate(*this, p, config))
                c.blocked = true;
            for (size_t i = 0; i < 15; ++i)
                if (!state.contacts[i].active) {
                    state.contacts[i] = c;
                    history[i] = h;
                    break;
                }
        }
}
} // namespace rb
