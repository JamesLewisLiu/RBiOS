#include "rb/Route.hpp"
#include <algorithm>
#include <cmath>

namespace rb {
namespace {
float distance(MotionPoint a, MotionPoint b) {
    const float dx = float(std::abs(int64_t(a.x) - b.x));
    const float dy = float(std::abs(int64_t(a.y) - b.y));
    const float square = float(double(dx) * dx + double(dy) * dy);
    return float(std::sqrt(double(square)));
}
} // namespace
MotionVector normalizeRouteDirection(MotionVector v) {
    const float square = float(double(v.x) * v.x + double(v.y) * v.y);
    const float difference = float(double(square) - 1.0);
    if (difference >= -1.1920928955078125e-7 && difference <= 1.1920928955078125e-7)
        return v;
    if (square < 1.1754943508222875e-38f)
        return {};
    const float length = float(std::sqrt(double(square)));
    const float reciprocal = float(1.0 / double(length));
    return {float(double(v.x) * reciprocal), float(double(v.y) * reciprocal)};
}
namespace {
int destinationLane(int32_t x, int32_t side, const FieldGeometry &field) {
    const int32_t px = side == 0 ? 768 - x : x;
    const int32_t eighth = (field.x(1) - field.x(0)) / 8;
    for (int lane = 0; lane < 7; ++lane) {
        const float threshold = float((double(lane) + 1.5) * float(eighth) + float(field.x(0)));
        if (threshold > double(float(px)))
            return lane;
    }
    return 0;
}
constexpr int destinationOffsets[3] = {-2, 0, 2};
constexpr int laneOffsets[7] = {-2, -1, -1, 0, 1, 1, 2};
constexpr int bounceOffsets[7] = {0, 0, 1, 1, 1, 2, 2};
void redirectDefault(RouteInput &r, ChartRouteConfig &c, const FieldGeometry &f, bool retry) {
    if (c.noteVariant) {
        const unsigned index = uint32_t(c.destination - 25) <= 2 ? unsigned(c.destination - 25) : 0;
        const int delta =
            destinationOffsets[index] - laneOffsets[destinationLane(r.start.x, c.side, f)];
        r.intermediates[0] =
            c.direction ? std::clamp(delta + 18, 17, 19) : std::clamp(2 - delta, 1, 3);
    } else {
        const uint32_t target = uint32_t(c.direction ? r.destination - 9 : 15 - r.destination);
        const int offset = bounceOffsets[target <= 6 ? target : 0];
        r.intermediates[0] =
            c.direction ? std::clamp(19 - offset, 17, 19) : std::clamp(3 - offset, 1, 3);
        r.intermediates[1] =
            c.direction ? std::clamp(8 - offset, 6, 8) : std::clamp(24 - offset, 22, 24);
    }
    if (!retry && !c.side && (r.intermediates[1] == 8 || r.intermediates[1] == 24)) {
        c.direction = c.direction == 0 ? 1 : c.direction == 1 ? 0 : -1;
        redirectDefault(r, c, f, true);
    }
}
void redirectAlternate(RouteInput &r, const ChartRouteConfig &c, const FieldGeometry &f) {
    const int lane = destinationLane(r.start.x, c.side, f);
    if (!c.noteVariant) {
        constexpr int offsets[7] = {0, 1, 1, 2, 2, 3, 3};
        r.intermediates[0] = std::clamp(lane + (c.direction ? 29 : 27), 28, 34);
        const uint32_t target = uint32_t(c.direction ? 15 - r.destination : r.destination - 9);
        const int b = (c.direction ? 22 : 6) - offsets[target <= 6 ? target : 0];
        r.intermediates[1] = c.direction ? std::clamp(b, 17, 20) : std::clamp(b, 1, 4);
    } else {
        const unsigned index = uint32_t(c.destination - 25) <= 2 ? unsigned(c.destination - 25) : 0;
        const int delta = destinationOffsets[index] - laneOffsets[lane];
        r.intermediates[0] =
            std::clamp(lane + 28 + (c.direction ? (delta > 0) : -(delta < 0)), 28, 34);
    }
}
} // namespace
void redirectRoute(RouteInput &r, ChartRouteConfig &c, MotionPoint start, const FieldGeometry &f) {
    r.start = start;
    const uint32_t which = uint32_t(c.overrideKind) <= 2 ? uint32_t(c.overrideKind) : 0;
    if (which == 1)
        redirectAlternate(r, c, f);
    else if (which == 2) {
        r.intermediates[0] = -1;
        r.intermediates[1] = -1;
    } else
        redirectDefault(r, c, f, false);
}
ChartRouteConfig configureChartRoute(const ChartRouteInput &in) {
    ChartRouteConfig out;
    out.start = in.previousTarget;
    out.noteVariant = in.noteVariant;
    out.side = in.side;
    out.pathKind = in.pathKind;
    out.parameter = in.parameter;
    out.destination = in.destination;
    out.direction = in.direction;
    out.specialCounter = in.pathKind == 1 ? in.specialCounter : -1;
    if (in.pathKind >= 7 && in.pathKind <= 9)
        out.pathClass = in.noteVariant == 1 ? 1 : 2;
    else if (in.noteVariant != 1 && (in.pathKind == 1 || in.pathKind == 2 || in.pathKind == 4 ||
                                     in.pathKind == 5 || in.pathKind == 6))
        out.pathClass = 1;
    return out;
}
RouteInput initializeRoute(const RouteSeedConfig &c, const FieldGeometry &field) {
    RouteInput out;
    out.start = c.start;
    out.start.x = std::clamp(out.start.x, field.x(0) + 1, field.x(1) - 1);
    out.destination = c.destination;
    out.side = c.side;
    out.kind = c.kind;
    out.widthMode = c.widthMode;
    out.suppressWallReflection = c.suppressWallReflection;
    if (c.pathClass && c.destination >= 9 && c.destination <= 15) {
        const int32_t px = c.side == 0 ? 768 - out.start.x : out.start.x;
        const int32_t eighth = (field.x(1) - field.x(0)) / 8;
        int lane = 0;
        while (lane < 7) {
            const float threshold = float((double(lane) + 1.5) * float(eighth) + float(field.x(0)));
            if (threshold > double(float(px)))
                break;
            ++lane;
        }
        if (lane == 7)
            lane = 0;
        constexpr int32_t offsets[7] = {-2, -2, -1, 0, 1, 2, 2};
        const int32_t delta = offsets[c.destination - 9] - offsets[lane];
        out.intermediates[0] =
            c.direction ? std::clamp(delta + 19, 18, 21) : std::clamp(3 - delta, 2, 5);
    }
    return out;
}
BuiltRoute buildRoute(const RouteInput &in, const FieldGeometry &field,
                      const RouteNormalizer &normalizer) {
    BuiltRoute out;
    auto &r = out.route;
    r.points[0] = in.start;
    r.order.push_back(0);
    for (auto selector : in.intermediates) {
        if (selector == -1)
            break;
        r.points[++r.intermediateCount] =
            field.destination(in.widthMode, uint32_t(selector), in.side);
        r.order.push_back(r.intermediateCount);
    }
    r.points[4] = field.destination(in.widthMode, uint32_t(in.destination), in.side);
    r.order.push_back(4);
    out.wallReflection = !in.suppressWallReflection && in.kind != 1;
    const auto previous = r.points[r.intermediateCount];
    MotionVector direction{float(int64_t(r.points[4].x) - previous.x),
                           float(int64_t(r.points[4].y) - previous.y)};
    direction = normalizer ? normalizer(direction) : normalizeRouteDirection(direction);
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y))
        throw Error("nonfinite route direction");
    auto hit = field.firstBoundary(r.points[4], direction.x, direction.y);
    // Original initializes the output slots to zero; firstBoundary preserves its
    // input on a miss, so retain zero explicitly in that branch.
    MotionPoint exit = hit.boundary == Boundary::none ? MotionPoint{} : hit.point;
    if (int(hit.boundary) < 2) {
        if (out.wallReflection) {
            r.points[5] = exit;
            r.order.push_back(5);
            hit = field.firstBoundary(exit, -direction.x, direction.y);
            // Original second query reuses its previous output on a miss.
            if (hit.boundary != Boundary::none)
                exit = hit.point;
        }
    } else
        out.wallReflection = false;
    r.points[6] = exit;
    r.order.push_back(6);
    float length = 0;
    uint32_t prior = 0;
    for (uint32_t i = 1; i <= r.intermediateCount; ++i) {
        length = float(double(distance(r.points[prior], r.points[i])) + length);
        prior = i;
    }
    length = float(double(distance(r.points[prior], r.points[4])) + length);
    for (size_t i = 0; i + 1 < r.order.size(); ++i) {
        const float part = distance(r.points[r.order[i]], r.points[r.order[i + 1]]);
        const float fraction = length == 0 ? 0 : float(double(part) / length);
        out.cumulativeFractions[i + 1] = float(double(out.cumulativeFractions[i]) + fraction);
    }
    return out;
}
} // namespace rb
