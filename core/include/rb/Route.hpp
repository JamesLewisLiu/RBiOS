#pragma once
#include "Motion.hpp"
#include <functional>

namespace rb {
// Preprocessed motion+68 state passed to 0x100A46C0, not raw chart fields.
struct RouteInput {
    MotionPoint start;
    std::array<int32_t, 3> intermediates{-1, -1, -1};
    int32_t destination = 0;
    int32_t side = 0, kind = 0, widthMode = -1;
    bool suppressWallReflection = false; // chart+284 == 1
};
// Standard non-linked initialization at 0x100A3270. Config corresponds to
// motion+24; class is +8, side is +16, target is +32, direction is +36.
struct RouteSeedConfig {
    MotionPoint start;
    int32_t pathClass = 0, side = 0, destination = 0, direction = 0;
    int32_t kind = 0, widthMode = -1;
    bool suppressWallReflection = false;
};
RouteInput initializeRoute(const RouteSeedConfig &, const FieldGeometry & = {});
struct ChartRouteInput {
    int32_t noteVariant = 0;                          // chart+284
    int32_t side = 0, destination = 0, direction = 0; // chart+328/+360/+364
    int32_t pathKind = 0, parameter = 0, specialCounter = -1;
    MotionPoint previousTarget; // motion+124/+128, overwrites the note position
};
// Eleven words originally stored at motion+24, without any pointer overlay.
struct ChartRouteConfig {
    MotionPoint start;
    int32_t pathClass = 0, noteVariant = 0, side = 0, pathKind = 0;
    int32_t specialCounter = -1, parameter = 0, destination = 0, direction = 0;
    int32_t overrideKind = -1;
};
ChartRouteConfig configureChartRoute(const ChartRouteInput &); // 0x100A3380
void redirectRoute(RouteInput &, ChartRouteConfig &, MotionPoint,
                   const FieldGeometry & = {}); // 0x100A3210 + 0x100A3070/0x100A2F10
struct BuiltRoute {
    MotionRoute route;
    bool wallReflection = false;
    std::array<float, 7> cumulativeFractions{}; // original +92..116; final slot unused
};
// Restored scalar fallback from d3dx9_41.dll 0x101D54A4. CPU-specific SIMD
// dispatch alternatives remain unverified; normalization can be overridden.
using RouteNormalizer = std::function<MotionVector(MotionVector)>;
MotionVector normalizeRouteDirection(MotionVector);
BuiltRoute buildRoute(const RouteInput &, const FieldGeometry & = {},
                      const RouteNormalizer & = {}); // 0x100A46C0 + 0x100A45C0
} // namespace rb
