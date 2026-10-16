#include "rb/MusicSheet.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <unordered_set>

namespace rb {
namespace {
int32_t signedByte(uint8_t n) { return n < 128 ? n : int32_t(n) - 256; }
int32_t lowWord(int32_t n) { auto v = uint32_t(n) & 65535; return v < 32768 ? v : int32_t(v) - 65536; }
int32_t highWord(int32_t n) { return lowWord(int32_t(uint32_t(n) >> 16)); }
int32_t checked(int64_t n) {
    if (n < INT32_MIN || n > INT32_MAX) throw Error("sheet arithmetic outside supported range");
    return int32_t(n);
}
void extend(DestinationReservation &r, unsigned slot, int32_t begin, int32_t end) {
    if (r.end[slot] > begin) { if (r.end[slot] < end) r.end[slot] = end; }
    else { r.begin[slot] = begin; r.end[slot] = end; }
}
std::vector<int32_t> shuffle(size_t count, OriginalRandom &random) {
    std::vector<int32_t> result(count);
    std::iota(result.begin(), result.end(), 0);
    for (size_t i = 1; i < count; ++i)
        std::swap(result[i], result[(random.next() & 0x7fffffff) % (i + 1)]);
    return result;
}
unsigned originalSide(int32_t side) { return uint32_t(side) <= 1 ? unsigned(side) : 0; }
int mappedDestination(int32_t destination) {
    constexpr int map[] = {6, 5, 4, 3, 2, 1, 0, 9, 8, 7};
    switch (destination) {
    case -2: case -1: return -2;
    case -3: return -4;
    case -4: return -3;
    default: return map[uint32_t(destination) <= 9 ? destination : 0];
    }
}
} // namespace
DestinationMaker::DestinationMaker(uint32_t seed, size_t lanes) : random(OriginalRandom::seeded(seed)) {
    if (lanes != 3 && lanes != 7) throw Error("unsupported original destination lane count");
    for (auto &side : reservations) side.resize(lanes);
}
void DestinationMaker::reserveTop(int32_t lane, int32_t time, int32_t duration, unsigned side,
                                   bool adjacent) {
    if (side >= 2) throw Error("invalid destination side");
    if (lane < 0 || lane > 2) return;
    size_t index = size_t(lane) * 3;
    auto &r = reservations[side];
    if (index >= r.size()) throw Error("top reservation outside lane allocation");
    auto end = checked(int64_t(time) + duration);
    extend(r[index], 1, time, end);
    if (adjacent) {
        if (index) extend(r[index - 1], 1, time, end);
        if (index + 1 < r.size()) extend(r[index + 1], 1, time, end);
    }
}
int32_t DestinationMaker::choose(int32_t duration, int32_t time, unsigned side, bool topVariant,
                                  const std::array<uint8_t, 10> &allowed) {
    if (side >= 2) throw Error("invalid destination side");
    if (duration <= 0) return -1;
    auto &r = reservations[side];
    for (auto &lane : r)
        for (unsigned i = 0; i < 3; ++i)
            if (time > lane.end[i]) lane.begin[i] = lane.end[i] = -99999;
    const int32_t end = checked(int64_t(time) + duration);
    std::array<std::vector<int32_t>, 3> groups;
    for (size_t i = 0; i < r.size(); ++i) {
        unsigned group = time <= r[i].end[1] && r[i].begin[1] <= end ? 1 : 0;
        if (time <= r[i].end[2] && r[i].begin[2] <= end) group = 2;
        groups[group].push_back(int32_t(i));
    }
    for (const auto &group : groups) {
        if (std::none_of(group.begin(), group.end(), [&](int i) { return allowed[size_t(i)]; })) continue;
        const auto order = shuffle(group.size(), random);
        int32_t chosen = -1;
        for (auto index : order) if (allowed[size_t(group[size_t(index)])]) { chosen = group[size_t(index)]; break; }
        if (chosen < 0) throw Error("original destination selection failed");
        extend(r[size_t(chosen)], 2, time, end);
        if (chosen) extend(r[size_t(chosen - 1)], 1, time, end);
        if (size_t(chosen + 1) < r.size()) extend(r[size_t(chosen + 1)], 1, time, end);
        extend(r[size_t(chosen)], 1, end, checked(int64_t(end) + (topVariant ? 10 : 30)));
        return chosen;
    }
    return 0;
}
int32_t SheetObject::longDuration(const Chart &chart) const {
    if (attributes[3] == 1) return duration;
    if (attributes[3] != 4) return 0;
    if (extraIndices.empty()) throw Error("switch note has no original extra group");
    const auto &e = chart.extras.at(extraIndices.back());
    return millisecondsToTicks(checked(int64_t(e.timeMs) + e.durationMs - source.travelMs - source.startMs));
}
MusicSheet MusicSheet::load(Chart chart, const SheetOptions &options) {
    if (chart.version != 20 || options.localChartSide < 0 || options.localChartSide > 1 || options.difficulty < 0 || options.difficulty > 3)
        throw Error("unsupported original sheet configuration");
    MusicSheet out;
    out.chart = std::move(chart); out.options = options;
    out.objects.reserve(out.chart.objects.size());
    for (size_t i = 0; i < out.chart.objects.size(); ++i) {
        const auto &source = out.chart.objects[i];
        SheetObject o; o.source = source;
        for (unsigned j = 0; j < 4; ++j) o.attributes[j] = signedByte(source.attributes[j]);
        o.start = source.startTick(); o.travel = source.travelTicks();
        o.side = options.localChartSide != o.attributes[1]; o.oppositeSide = !o.side;
        o.duration = source.durationTicks(); o.rawDestination = source.parameters[1];
        o.destination = o.attributes[2] == 1 ? o.rawDestination + 7 : mappedDestination(o.rawDestination);
        o.directionFlags = source.parameters[2];
        if (source.flags & 8) { o.previous = lowWord(source.extension[0]); o.next = highWord(source.extension[0]); o.linkParameter = source.extension[1]; }
        if (o.attributes[3] == 3) {
            switch (o.destination) {
            case 0: o.destination = 1; o.widthMode = 0; break;
            case 1: case 2: o.destination = 1; o.widthMode = 1; break;
            case 4: case 5: o.destination = 5; o.widthMode = 1; break;
            case 6: o.destination = 5; o.widthMode = 0; break;
            default: o.destination = 3; o.widthMode = 1; break;
            }
        }
        out.objects.push_back(std::move(o));
    }
    // The original loader accepts only the first contiguous group for an object.
    int32_t previous = -1; SheetObject *group = nullptr;
    for (size_t i = 0; i < out.chart.extras.size(); ++i) {
        const auto object = out.chart.extras[i].object;
        if (object != previous || !group) {
            previous = object; group = nullptr;
            if (object >= 0 && size_t(object) < out.objects.size() && out.objects[size_t(object)].extraIndices.empty()) group = &out.objects[size_t(object)];
        }
        if (group) group->extraIndices.push_back(i);
    }
    auto &counts = out.counts;
    int64_t travelSum = 0; int32_t travelCount = 0, minTravel = 0, maxTravel = 0;
    for (auto &o : out.objects) {
        auto side = originalSide(o.attributes[1]); o.index = counts.objects[side]++;
        ++counts.total[side];
        if (o.source.flags & 0x20) ++counts.justReflec[side];
        if (!side) { travelSum += o.source.travelMs; ++travelCount; if (!minTravel || o.source.travelMs < minTravel) minTravel = o.source.travelMs; if (!maxTravel || o.source.travelMs > maxTravel) maxTravel = o.source.travelMs; }
        int32_t additional = 0;
        auto ticks = [](int32_t d) { return d >= 30 ? (d - 30) / 10 + 1 : 0; };
        if (o.attributes[3] == 1) additional = 1 + ticks(o.duration);
        else if (o.attributes[3] == 4) {
            int32_t previousTick = o.targetTick();
            for (auto e : o.extraIndices) { const auto &extra = out.chart.extras[e]; const auto end = millisecondsToTicks(checked(int64_t(extra.timeMs) + extra.durationMs)); additional += ticks(end - previousTick); previousTick = end; }
        }
        counts.total[side] += additional; counts.longTicks[side] += additional;
    }
    for (const auto &extra : out.chart.extras) { auto side = originalSide(out.objects.at(size_t(extra.object)).attributes[1]); ++counts.total[side]; ++counts.extra[side]; }
    if (travelCount > 0 && travelSum > 0 && minTravel > 0 && maxTravel > 0) {
        counts.speedAverage = checked(500000LL * travelCount / travelSum);
        counts.speedMinimum = 500000 / maxTravel; counts.speedMaximum = 500000 / minTravel;
    }
    if (!options.unequalSidesAllowed && counts.total[0] != counts.total[1]) throw Error("original chart side event counts differ");
    return out;
}
void MusicSheet::prepare() {
    auto objectAt = [&](int32_t index) -> SheetObject & {
        if (index < 0 || size_t(index) >= objects.size()) throw Error("original note link outside sheet");
        return objects[size_t(index)];
    };
    for (auto &o : objects) {
        o.wide = options.difficulty < 2;
        o.busyTicks = 0;
        const auto kind = o.attributes[3];
        if (kind == 1 || kind == 4) o.busyTicks = checked(int64_t(o.longDuration(chart)) + (o.attributes[2] == 1 ? 7 : 9));
        else if (!(o.source.flags & 8)) o.busyTicks = 7;
        else if (o.previous == -1) {
            auto *last = &o; std::unordered_set<SheetObject *> visited{last};
            while (last->next >= 0) { last = &objectAt(last->next); if (!visited.insert(last).second) throw Error("cyclic original sheet links"); }
            o.busyTicks = checked(int64_t(last->targetTick()) - o.targetTick() + (last->attributes[3] == 1 ? last->duration + (o.attributes[2] == 1 ? 7 : 9) : 7));
        }
        o.allowedDestinations.fill(0); o.fixedDestination = -1;
        int32_t target = o.destination;
        if (o.attributes[2] == 1) { constexpr int top[] = {7, 8, 9}; target = top[uint32_t(o.rawDestination) <= 2 ? o.rawDestination : 0]; }
        else if (uint32_t(target) > 9) continue;
        if (target != -1) { o.fixedDestination = target; o.allowedDestinations[uint32_t(target) < 10 ? size_t(target) : 0] = 1; }
    }
    auto nextDirectional = [&](int32_t index, int32_t side, int32_t time, int32_t destination) -> SheetObject * {
        int32_t previousTime = -1;
        for (int32_t i = index; size_t(i) < objects.size(); ++i) {
            auto &n = objects[size_t(i)];
            if (n.side != side || ((n.source.flags & 8) && n.previous != -1)) continue;
            auto at = n.targetTick();
            if (at == time) continue;
            if (previousTime != -1 && previousTime >= at) return nullptr;
            if (n.destination == destination) return &n;
            previousTime = time;
        }
        return nullptr;
    };
    for (auto &o : objects) {
        if (o.attributes[2] == 1 || uint32_t(o.destination) <= 9 || ((o.source.flags & 8) && o.previous != -1)) continue;
        o.allowedDestinations = {1,1,1,1,1,1,1,0,0,0};
        for (const auto &other : objects) {
            if (&other == &o || other.side != o.side || o.targetTick() >= other.targetTick() + other.busyTicks || o.targetTick() + o.busyTicks <= other.targetTick()) continue;
            if (other.fixedDestination != -1) {
                o.allowedDestinations[uint32_t(other.fixedDestination) < 10 ? size_t(other.fixedDestination) : 0] = 0;
                if (other.attributes[3] == 1 && other.attributes[2] == 1) { constexpr int adjacent[] = {0,3,6}; o.allowedDestinations[size_t(adjacent[uint32_t(other.rawDestination) <= 2 ? other.rawDestination : 0])] = 0; }
            }
        }
        for (int dir : {-3,-4}) {
            int count = 0; auto *current = nextDirectional(o.source.id, o.side, o.targetTick(), dir);
            std::unordered_set<SheetObject *> visited;
            while (current) { if (!visited.insert(current).second) throw Error("cyclic destination constraints"); ++count; current = nextDirectional(current->source.id,current->side,current->targetTick(),dir); }
            for (int j = 0; j < count; ++j) { int32_t lane = dir == -3 ? j : 6-j; o.allowedDestinations[uint32_t(lane) < 10 ? size_t(lane) : 0] = 0; }
        }
    }
    DestinationMaker maker(options.seed);
    for (size_t i = 0; i < objects.size(); ++i) {
        auto &o = objects[i];
        if ((o.source.flags & 8) && o.previous != -1) o.routeDestination = objectAt(o.previous).routeDestination;
        else if (o.attributes[2] != 1) {
            if (o.source.flags & 0x40) {
                std::array<bool,3> occupied{}; int32_t first = -1;
                for (size_t j = i; j < objects.size(); ++j) { const auto &n = objects[j]; if (n.side == o.side && n.targetTick() >= o.targetTick() && n.targetTick() <= o.targetTick() + o.duration && n.attributes[2] == 1) { if (n.rawDestination < 0 || n.rawDestination > 2) throw Error("invalid original top lane"); if (first == -1) first = n.rawDestination; occupied[size_t(n.rawDestination)] = true; } }
                const bool all = occupied[0] && occupied[1] && occupied[2];
                for (int j = 0; j < 3; ++j) if (occupied[size_t(j)] && (!all || first != j)) maker.reserveTop(j,o.targetTick(),o.busyTicks,unsigned(o.side),true);
            }
            o.routeDestination = maker.choose(o.busyTicks,o.targetTick(),unsigned(o.side),o.attributes[2] != 0,o.allowedDestinations) + 9;
        } else {
            o.routeDestination = o.rawDestination == 0 ? 25 : o.rawDestination == 2 ? 26 : 27;
            if (o.rawDestination >= 0 && o.rawDestination <= 2) maker.reserveTop(o.rawDestination,o.targetTick(),o.busyTicks,unsigned(o.side),false);
        }
    }
    auto directions = OriginalRandom::seeded(options.seed);
    constexpr int lanes[] = {0,1,2,3,4,5,6,3,3,3};
    constexpr int mirror[] = {6,5,4,3,2,1,0};
    for (auto &o : objects) {
        auto direction = int32_t(directions.next() & 1);
        for (auto index : o.source.links) objectAt(index).routeDirection = direction;
        if (o.source.group != -1) continue;
        int32_t lane = int32_t((directions.next() & 0x7fffffff) % 7);
        const auto kind = o.attributes[3];
        if ((uint32_t(o.destination) <= 9 && !(o.directionFlags & 2) && kind != 3) || kind == 3 || o.attributes[2] == 1) {
            auto mapped = mappedDestination(o.destination);
            lane = lanes[uint32_t(mapped) < 10 ? size_t(mapped) : 0];
            if (o.side == 1) lane = mirror[uint32_t(lane) < 7 ? size_t(lane) : 0];
        }
        o.launchLane = lane;
    }
    initialObjects.clear();
    for (size_t i = 0; i < objects.size(); ++i) if (objects[i].source.flags & 0x10) initialObjects.push_back(i);
    std::sort(initialObjects.begin(),initialObjects.end(),[&](size_t a,size_t b) { const auto &x=objects[a].source; const auto &y=objects[b].source; return x.startMs != y.startMs ? x.startMs < y.startMs : x.id < y.id; });
    counts.afterJustReflec = {};
    std::array<int32_t,2> last{}; std::array<bool,2> found{};
    for (auto i=objects.rbegin(); i!=objects.rend(); ++i) { auto side=originalSide(i->attributes[1]); if (!found[side] && (i->source.flags & 0x20)) { last[side]=i->targetTick(); found[side]=true; } }
    for (const auto &o : objects) { auto side=originalSide(o.attributes[1]); if (last[side] < o.targetTick()) ++counts.afterJustReflec[side]; }
    for (const auto &e : chart.extras) { auto side=originalSide(objects.at(size_t(e.object)).attributes[1]); if (last[side] < millisecondsToTicks(e.timeMs) + millisecondsToTicks(e.durationMs)) ++counts.afterJustReflec[side]; }
}
} // namespace rb
