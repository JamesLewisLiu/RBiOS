#include "rb/Chart.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace rb {
int32_t millisecondsToTicks(int32_t ms) {
    return static_cast<int32_t>(double(ms) * 60.0 / 1000.0);
}
int32_t ChartObject::startTick() const {
    return std::max(-150, millisecondsToTicks(startMs));
}
int32_t ChartObject::travelTicks() const {
    return millisecondsToTicks(travelMs);
}
int32_t ChartObject::durationTicks() const {
    return millisecondsToTicks(parameters[0]);
}
int ChartObject::destination() const {
    if (attributes[2] == 1)
        return int(parameters[1]) + 7;
    // 0x10292634, checked from original PE by audit tool before use.
    constexpr int map[10] = {6, 5, 4, 3, 2, 1, 0, 9, 8, 7};
    switch (parameters[1]) {
    case -2:
    case -1:
        return -2;
    case -3:
        return -4;
    case -4:
        return -3;
    default:
        return map[parameters[1] >= 0 && parameters[1] <= 9 ? parameters[1] : 0];
    }
}
std::array<uint32_t, 4> randomSeed(uint32_t seed) {
    std::array<uint32_t, 4> out{};
    for (uint32_t i = 0; i < 4; ++i) {
        seed = 1812433253u * (seed ^ (seed >> 30)) + i;
        out[i] = seed;
    }
    return out;
}
Chart Chart::parse(const Bytes &bytes) {
    Reader r(bytes);
    if (r.u32() != 0x46464252)
        throw Error("not RBFF");
    Chart c;
    c.version = r.u32();
    if (c.version != 20)
        throw Error("unsupported RBFF version: " + std::to_string(c.version));
    r.skip(8);
    c.bpm = r.f32();
    c.durationMs = r.i32();
    c.unknownTime = r.i32();
    auto objects = r.u16();
    auto commands = r.u16();
    c.declaredEvents = r.u16();
    c.unknownCount = r.u16();
    auto extras = r.u32();
    c.unknownHeader = r.u32();
    if (!std::isfinite(c.bpm) || c.bpm <= 0 || c.durationMs < 0)
        throw Error("invalid chart timing");
    if (objects > 32767 || extras > 1000000)
        throw Error("chart count exceeds supported range");
    c.objects.reserve(objects);
    for (size_t i = 0; i < objects; ++i) {
        ChartObject o;
        o.startMs = r.i32();
        o.travelMs = r.i32();
        o.id = r.i16();
        o.group = r.i16();
        auto links = r.i16();
        if (links < 0 || links > 64)
            throw Error("invalid object link count");
        for (int j = 0; j < links; ++j)
            o.links.push_back(r.i16());
        for (auto &a : o.attributes)
            a = r.u8();
        for (auto &p : o.parameters)
            p = r.i16();
        o.flags = r.u32();
        for (auto &b : o.tailBytes)
            b = r.u8();
        o.tailWord = r.i16();
        o.tailValue = r.i32();
        if (o.flags & 8)
            for (auto &e : o.extension)
                e = r.i32();
        c.objects.push_back(std::move(o));
    }
    for (size_t i = 0; i < commands; ++i) {
        ChartCommand cmd;
        cmd.type = r.i16();
        cmd.reserved = r.i16();
        cmd.timeMs = r.i32();
        for (auto &f : cmd.fields)
            f = r.i32();
        c.commands.push_back(cmd);
    }
    for (size_t i = 0; i < extras; ++i) {
        ChartExtra e;
        e.object = r.i16();
        e.type = r.i16();
        e.timeMs = r.i32();
        e.durationMs = r.i32();
        e.value = r.i32();
        if (e.object < 0 || size_t(e.object) >= c.objects.size())
            throw Error("extra references absent object");
        c.extras.push_back(e);
    }
    if (r.remaining())
        throw Error("unparsed chart trailing bytes: " + std::to_string(r.remaining()));
    for (const auto &o : c.objects)
        for (auto link : o.links)
            if (link < 0 || size_t(link) >= c.objects.size())
                throw Error("object link out of range");
    return c;
}
int32_t Chart::additionalTicks(size_t index) const {
    const auto &o = objects.at(index);
    int total = 0;
    auto count = [](int t) { return t >= 30 ? (t - 30) / 10 + 1 : 0; };
    if (o.attributes[3] == 1)
        return 1 + count(o.durationTicks());
    if (o.attributes[3] == 4)
        for (auto duration : switchDurations(index))
            total += count(duration);
    return total;
}
std::vector<int32_t> Chart::switchDurations(size_t index) const {
    const auto &o = objects.at(index);
    int64_t previous = int64_t(o.startTick()) + o.travelTicks();
    std::vector<int32_t> result;
    for (const auto &e : extras)
        if (e.object == int(index)) {
            int64_t endMs = int64_t(e.timeMs) + e.durationMs;
            if (endMs < INT32_MIN || endMs > INT32_MAX)
                throw Error("switch segment time exceeds int32");
            int32_t end = millisecondsToTicks(int32_t(endMs));
            int64_t duration = int64_t(end) - previous;
            if (duration < INT32_MIN || duration > INT32_MAX)
                throw Error("switch duration exceeds int32");
            result.push_back(int32_t(duration));
            previous = end;
        }
    return result;
}
} // namespace rb
