#pragma once
#include "Binary.hpp"
#include <array>
#include <optional>

namespace rb {
struct MotionVector {
    float x = 0, y = 0;
};
struct MotionPoint {
    int32_t x = 0, y = 0;
};
enum class Boundary : int32_t { left = 0, right = 1, top = 2, bottom = 3, none = 4 };
struct MotionRoute {
    std::array<MotionPoint, 7> points{};
    uint32_t intermediateCount = 0;
    // Includes the terminal point. Original path stores one fewer in +120.
    std::vector<uint32_t> order;
};
struct MotionSegment {
    MotionVector velocity;
    float begin = 0, end = 0;
};
struct MotionTimeline {
    std::vector<MotionSegment> segments;
    float speed = 0;
    MotionVector velocity;
    int32_t index = 0, previousIndex = 0, tick = 0;
    bool paused = false;
    int32_t pauseTicks = 0;
    std::array<float, 3> reflectionTimes{-9999.9004f, -9999.9004f, -9999.9004f};
    std::array<Boundary, 3> reflectionDirections{Boundary::none, Boundary::none, Boundary::none};
    float specialReflectionTime = -9999.9004f;
    int32_t specialReflectionDirection = 4;
    static MotionTimeline initialize(const MotionRoute &, int32_t durationTicks,
                                     uint32_t reflectionCount); // 0x100A42D0
    void advance();                         // 0x100A3EA0, velocity selection/blending at one tick
    MotionVector effectiveVelocity() const; // 0x100A3E40
};
struct MotionPosition {
    MotionVector position, delta, previousDelta;
    MotionPoint rounded;
    bool suppressOnce = false;
    // Position-only branch of 0x1015A240. Offscreen callbacks remain external.
    void integrate(const MotionTimeline &);
};
struct BoundaryHit {
    Boundary boundary = Boundary::none;
    MotionPoint point;
};
// Original field globals initialized by 0x100A4C90. Logical pixel coordinates,
// independent of UIKit points or the user-selected display fraction.
struct FieldGeometry {
    int32_t left = 0, right = 768, top = 80, bottom = 1222;
    int32_t upperLine = 204, lowerLine = 1098, innerLeft = 128, innerRight = 640;
    int32_t middleUpper = 431, middleLower = 871;
    int32_t bottomOffset = 0, topOffset = 0, laneOffset = 0;
    int32_t x(unsigned index) const;                // 0x100A4C10, defined indices 0..7
    int32_t y(unsigned index) const;                // 0x100A4B90, defined indices 0..7
    int32_t laneX(uint32_t index) const;            // 0x100A51B0
    int32_t laneY(uint32_t index, int side) const;  // 0x100A5140
    int32_t region(int32_t x, int alternate) const; // 0x100A50B0
    MotionPoint destination(int wide, uint32_t index, int side) const; // 0x100A53C0
    std::optional<MotionPoint> verticalIntersection(MotionPoint, float vx, float vy,
                                                    int side) const; // 0x100A4D40
    std::optional<MotionPoint> horizontalIntersection(MotionPoint, float vx, float vy,
                                                      int side) const; // 0x100A4DF0
    BoundaryHit firstBoundary(MotionPoint, float vx, float vy) const;  // 0x100A4E90
};
} // namespace rb
