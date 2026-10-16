#pragma once
#include "Binary.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace rb {
struct Point {
    double x = 0, y = 0;
};
struct Rect {
    double x = 0, y = 0, width = 0, height = 0;
};
struct ViewportTransform {
    Rect display;
    double scale = 1;
    static ViewportTransform fit(Rect safe, double fraction) {
        if (!std::isfinite(fraction) || fraction < 0.5 || fraction > 1 ||
            !std::isfinite(safe.width) || !std::isfinite(safe.height) || safe.width <= 0 ||
            safe.height <= 0)
            throw Error("invalid viewport");
        double s = std::min(safe.width / 768.0, safe.height / 1360.0) * fraction;
        return {{safe.x + (safe.width - 768 * s) / 2, safe.y + (safe.height - 1360 * s) / 2,
                 768 * s, 1360 * s},
                s};
    }
    std::optional<Point> toGame(Point p) const {
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            return {};
        Point q{(p.x - display.x) / scale, (p.y - display.y) / scale};
        if (q.x < 0 || q.x >= 768 || q.y < 0 || q.y >= 1360)
            return {};
        return q;
    }
    Point toDisplay(Point p) const {
        return {display.x + p.x * scale, display.y + p.y * scale};
    }
};
// Owns stable IDs, including release on leaving viewport. UIKit object addresses never cross ABI.
class TouchState {
    std::map<uint64_t, Point> points_;

  public:
    void update(uint64_t id, std::optional<Point> p) {
        if (p)
            points_[id] = *p;
        else
            points_.erase(id);
    }
    void reset() {
        points_.clear();
    }
    const std::map<uint64_t, Point> &points() const {
        return points_;
    }
};
} // namespace rb
