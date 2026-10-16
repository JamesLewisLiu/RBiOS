#include "rb/Motion.hpp"
#include <algorithm>
#include <cmath>

namespace rb {
namespace {
float distance(MotionPoint a, MotionPoint b) {
    float dx = float(std::abs(int64_t(a.x) - b.x)), dy = float(std::abs(int64_t(a.y) - b.y));
    const float square = float(double(dx) * dx + double(dy) * dy);
    return float(std::sqrt(double(square)));
}
} // namespace
MotionTimeline MotionTimeline::initialize(const MotionRoute &r, int32_t ticks,
                                          uint32_t reflections) {
    if (ticks <= 0 || r.intermediateCount > 3 || r.order.size() < 2 || r.order.size() > 7 ||
        reflections > 3 || reflections >= r.order.size() - 1)
        throw Error("invalid original motion route");
    for (auto i : r.order)
        if (i > 6)
            throw Error("motion route point out of range");
    MotionTimeline out;
    float length = 0;
    uint32_t prior = 0;
    for (uint32_t i = 1; i <= r.intermediateCount; ++i) {
        length = float(double(distance(r.points[prior], r.points[i])) + length);
        prior = i;
    }
    length = float(double(distance(r.points[prior], r.points[4])) + length);
    out.speed = float(double(length) / ticks);
    float elapsed = 0;
    for (size_t i = 0; i + 1 < r.order.size(); ++i) {
        const auto a = r.points[r.order[i]], b = r.points[r.order[i + 1]];
        const float duration = out.speed == 0 ? 0 : float(double(distance(a, b)) / out.speed);
        MotionSegment seg;
        seg.begin = elapsed;
        seg.end = float(double(duration) + elapsed);
        if (duration != 0) {
            seg.velocity = {float(double(b.x - a.x) / duration),
                            float(double(b.y - a.y) / duration)};
        }
        if (duration == 0 || (seg.velocity.x == 0 && seg.velocity.y == 0))
            seg.velocity = i ? out.segments.back().velocity : MotionVector{0, 1};
        out.segments.push_back(seg);
        elapsed = seg.end;
        if (r.order[i + 1] == 5) {
            out.specialReflectionTime = seg.end;
            out.specialReflectionDirection = seg.velocity.x >= 0;
        }
    }
    out.velocity = out.segments.front().velocity;
    for (uint32_t i = 0; i < reflections; ++i) {
        const auto a = out.segments[i].velocity, b = out.segments[i + 1].velocity;
        out.reflectionTimes[i] = out.segments[i].end;
        out.reflectionDirections[i] = double(a.y) * b.y < 0 ? (a.y < 0   ? Boundary::top
                                                               : a.y > 0 ? Boundary::bottom
                                                                         : Boundary::none)
                                                            : (a.x < 0   ? Boundary::left
                                                               : a.x > 0 ? Boundary::right
                                                                         : Boundary::none);
    }
    return out;
}
void MotionTimeline::advance() {
    if (paused) {
        if (pauseTicks-- == 1)
            paused = false;
        return;
    }
    if (segments.empty() || index < 0 || size_t(index) >= segments.size())
        throw Error("motion timeline has no current segment");
    ++tick;
    previousIndex = index;
    const auto &current = segments[size_t(index)];
    if (size_t(index) + 1 < segments.size() && current.end <= tick) {
        const float before = float(double(current.end) - (tick - 1)),
                    after = float(1.0 - double(before));
        const auto next = segments[size_t(index) + 1].velocity;
        const float x1 = float(double(current.velocity.x) * before),
                    y1 = float(double(current.velocity.y) * before);
        const float x2 = float(double(next.x) * after), y2 = float(double(next.y) * after);
        velocity = {float(double(x2) + x1), float(double(y2) + y1)};
        ++index;
    } else
        velocity = current.velocity;
}
MotionVector MotionTimeline::effectiveVelocity() const {
    return paused ? MotionVector{} : velocity;
}
void MotionPosition::integrate(const MotionTimeline &t) {
    if (suppressOnce) {
        suppressOnce = false;
        return;
    }
    previousDelta = delta;
    delta = t.effectiveVelocity();
    position.x = float(double(position.x) + delta.x);
    position.y = float(double(position.y) + delta.y);
    const double x = double(position.x) + .5, y = double(position.y) + .5;
    if (!std::isfinite(x) || !std::isfinite(y) || x < INT32_MIN || x > INT32_MAX || y < INT32_MIN ||
        y > INT32_MAX)
        throw Error("motion position outside int32 range");
    rounded = {int32_t(x), int32_t(y)};
}
int32_t FieldGeometry::x(unsigned i) const {
    switch (i) {
    case 0:
        return left;
    case 1:
        return right;
    case 2:
        return innerLeft;
    case 3:
    case 6:
        return (left + right) / 2;
    case 4:
        return innerRight;
    case 5:
        return left + 72;
    case 7:
        return right - 72;
    default:
        throw Error("undefined original X selector");
    }
}
int32_t FieldGeometry::y(unsigned i) const {
    switch (i) {
    case 0:
        return bottom + bottomOffset;
    case 1:
        return top - topOffset;
    case 2:
        return lowerLine - 72;
    case 3:
        return upperLine - topOffset + 72;
    case 4:
        return lowerLine;
    case 5:
        return upperLine - topOffset;
    case 6:
        return middleLower;
    case 7:
        return middleUpper - topOffset;
    default:
        throw Error("undefined original Y selector");
    }
}
int32_t FieldGeometry::laneX(uint32_t i) const {
    if (i > 5)
        i = 0;
    const float quarter = float(double(x(1) - x(0)) * 0.25);
    if (i < 3)
        return int32_t(float(double(x(0)) + double(quarter) * (i + 1)));
    return int32_t(float(x(i == 3 ? 2 : i == 4 ? 3 : 4)));
}
int32_t FieldGeometry::laneY(uint32_t i, int side) const {
    if (i >= 6)
        i = 0;
    const bool upper = i >= 3;
    if (side)
        return y(upper ? 7 : 5) + 80;
    return upper ? y(6) - 70 : y(4) - laneOffset - 70;
}
int32_t FieldGeometry::region(int32_t px, int alternate) const {
    const int32_t width = x(1) - x(0), relative = px - x(0);
    if (width < 3)
        throw Error("field width too small");
    if (alternate)
        return relative >= width / 3 ? (relative >= 2 * width / 3) + 4 : 3;
    return std::clamp(relative / (width / 3), 0, 2);
}
MotionPoint FieldGeometry::destination(int wide, uint32_t i, int side) const {
    int32_t px = 0, py;
    if (i <= 8)
        px = x(0) + 36;
    else if (uint32_t(i - 16) <= 8)
        px = x(1) - 36;
    else if ((i >= 9 && i <= 15) || (i >= 28 && i <= 34)) {
        const uint32_t lane = i <= 15 ? i - 9 : i - 28;
        const int delta = wide == 1 ? -64 : 0;
        px = lane < 3 ? 96 * int(lane + 1) + delta : lane == 3 ? 384 : 96 * int(lane + 1) - delta;
    } else if (i >= 25 && i <= 27)
        px = x(i == 25 ? 2 : i == 26 ? 4 : 3);
    if (side == 1)
        px = 768 - px;
    if (i >= 9 && i <= 15)
        py = y(side ? 4 : 5);
    else if (i >= 25 && i <= 27)
        py = y(side ? 6 : 7);
    else if (i >= 28 && i <= 34)
        py = y(side ? 1 : 0);
    else {
        const int32_t low = y(4), high = y(5);
        const uint32_t wall = i >= 16 && i <= 24 ? i - 16 : i;
        constexpr float factors[9] = {.38f, .30f, .20f, .10f, 0, -.10f, -.20f, -.30f, -.38f};
        float offset = wall <= 8 ? float(double(float(low - high)) * double(factors[wall])) : 0;
        if (side == 1)
            offset = -offset;
        py = int32_t(offset) + (low + high) / 2;
    }
    return {std::clamp(px, x(0), x(1)), std::clamp(py, y(1), y(0))};
}
std::optional<MotionPoint> FieldGeometry::verticalIntersection(MotionPoint p, float vx, float vy,
                                                               int side) const {
    if (!std::isfinite(vx) || !std::isfinite(vy))
        throw Error("nonfinite velocity");
    const int32_t wall = x(side ? 1 : 0) + (side ? -36 : 36);
    if (vx == 0)
        return {};
    const float t = float(double(wall - p.x) / double(vx));
    if (t < 0)
        return {};
    const double value = double(t) * double(vy) + p.y;
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return {};
    const int32_t py = int32_t(value);
    if (py < y(1) + 36 || py > y(0) - 36)
        return {};
    return MotionPoint{wall, py};
}
std::optional<MotionPoint> FieldGeometry::horizontalIntersection(MotionPoint p, float vx, float vy,
                                                                 int side) const {
    if (!std::isfinite(vx) || !std::isfinite(vy))
        throw Error("nonfinite velocity");
    const int32_t wall = side == 2 ? y(1) + 36 : y(0) - 36;
    if (vy == 0)
        return {};
    const float t = float(double(wall - p.y) / double(vy));
    if (t < 0)
        return {};
    const double value = double(t) * double(vx) + p.x;
    if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
        return {};
    const int32_t px = int32_t(value);
    if (px < x(0) + 36 || px > x(1) - 36)
        return {};
    return MotionPoint{px, wall};
}
BoundaryHit FieldGeometry::firstBoundary(MotionPoint p, float vx, float vy) const {
    if (vy != 0) {
        const auto which = vy < 0 ? Boundary::top : Boundary::bottom;
        if (auto hit = horizontalIntersection(p, vx, vy, int(which)))
            return {which, *hit};
    }
    if (vx != 0) {
        const auto which = vx < 0 ? Boundary::left : Boundary::right;
        if (auto hit = verticalIntersection(p, vx, vy, int(which)))
            return {which, *hit};
    }
    return {Boundary::none, p};
}
} // namespace rb
