#pragma once
#include "CPU.hpp"
#include "Chart.hpp"

namespace rb {
// Recovered CMusicSheet2 object fields. Pointer-bearing x86 records are replaced
// by indices; this is never an in-memory overlay of the Windows object.
struct SheetObject {
    ChartObject source;
    std::array<int32_t, 4> attributes{}; // signed byte expansion, chart+276
    int32_t start = 0, travel = 0, index = 0, side = 0, oppositeSide = 1;
    int32_t duration = 0, rawDestination = -1, destination = -2, widthMode = -1;
    int32_t previous = -1, next = -1, linkParameter = 0;
    int32_t busyTicks = 0, routeDestination = -1, routeDirection = -1, launchLane = -1;
    int32_t directionFlags = 0, fixedDestination = -1;
    bool wide = true;
    std::array<uint8_t, 10> allowedDestinations{};
    std::vector<size_t> extraIndices;
    int32_t targetTick() const { return start + travel; }
    int32_t longDuration(const Chart &) const; // 0x101493D0
};
struct SheetCounts {
    std::array<int32_t, 2> total{}, justReflec{}, extra{}, afterJustReflec{}, longTicks{}, objects{};
    int32_t speedAverage = 0, speedMinimum = 0, speedMaximum = 0;
};
struct SheetOptions {
    int32_t localChartSide = 0, difficulty = 0;
    uint32_t seed = 77;
    bool unequalSidesAllowed = false;
};
struct MusicSheet {
    Chart chart;
    std::vector<SheetObject> objects;
    SheetCounts counts;
    std::vector<size_t> initialObjects; // original flag 0x10, sorted by time and id
    SheetOptions options;
    static MusicSheet load(Chart, const SheetOptions &); // 0x1014A230 / 10149E90
    // Default original destination option (no top-lane RANDOM/S-RANDOM option).
    void prepare(); // logic from 0x1014B270 / 10149BB0 / 1014AD20 / 10149670
};
struct DestinationReservation {
    std::array<int32_t, 3> begin{{-99999, -99999, -99999}};
    std::array<int32_t, 3> end{{-99999, -99999, -99999}};
};
class DestinationMaker {
  public:
    OriginalRandom random;
    std::array<std::vector<DestinationReservation>, 2> reservations;
    explicit DestinationMaker(uint32_t seed, size_t lanes = 7); // 0x100A37E0
    void reserveTop(int32_t lane, int32_t time, int32_t duration, unsigned side,
                    bool adjacent); // 0x100A3720
    int32_t choose(int32_t duration, int32_t time, unsigned side, bool topVariant,
                   const std::array<uint8_t, 10> &allowed); // 0x100A38D0
};
} // namespace rb
