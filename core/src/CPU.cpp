#include "rb/CPU.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>

namespace rb {
namespace {
int32_t sub32(int32_t a, int32_t b) {
    uint32_t bits = uint32_t(a) - uint32_t(b);
    int32_t result;
    std::memcpy(&result, &bits, 4);
    return result;
}
// Explicit binary32 stores match the original x87 fstp boundaries and also
// prevent ARM64 fused arithmetic from changing the seeded replay.
float f32(double value) {
    volatile float rounded = float(value);
    return rounded;
}
void validCount(uint32_t count) {
    // Adapter allocation limit, not a claim about an original chart limit.
    if (count > 1000000)
        throw std::invalid_argument("CPU note count exceeds adapter allocation limit");
}
} // namespace
OriginalRandom OriginalRandom::seeded(uint32_t seed) {
    OriginalRandom r{};
    for (uint32_t i = 0; i < 4; ++i) {
        seed = 1812433253u * (seed ^ (seed >> 30)) + i;
        r.words[i] = seed;
    }
    return r;
}
uint32_t OriginalRandom::next() {
    const auto t = words[0] ^ (words[0] << 11);
    const auto w = words[3];
    words = {words[1], words[2], w, w ^ t ^ ((t ^ (w >> 11)) >> 8)};
    return words[3];
}
float OriginalRandom::range(float lower, float upper) {
    const auto unit = f32(double(next() & 0x7fff) / 32767.0);
    const auto scaled = f32((double(upper) - double(lower)) * double(unit));
    return f32(double(lower) + double(scaled));
}
bool OriginalRandom::chance(float percent) {
    return percent > range(0.0f, 100.0f);
}
CPUParameters cpuParameters(uint32_t difficulty) {
    static constexpr CPUParameters rows[] = {
        {0.50f, 0.56f, 0.2f},    {0.56f, 0.61f, 0.2f},    {0.61f, 0.66f, 0.2f},
        {0.66f, 0.72f, 0.5f},    {0.72f, 0.76f, 1.5f},    {0.76f, 0.79f, 3.0f},
        {0.79f, 0.82f, 6.0f},    {0.82f, 0.85f, 8.0f},    {0.85f, 0.88f, 12.0f},
        {0.88f, 0.91f, 15.0f},   {0.92f, 0.95f, 25.0f},   {0.95f, 0.965f, 35.0f},
        {0.965f, 0.975f, 40.0f}, {0.975f, 0.985f, 45.0f}, {0.985f, 0.990f, 50.0f},
        {0.990f, 0.995f, 60.0f}, {1.0f, 1.0f, 0.0f},      {1.0f, 1.0f, 100.0f},
        {0.72f, 0.76f, 0.0f}};
    if (difficulty >= 19)
        throw std::invalid_argument("CPU difficulty outside recovered table");
    return rows[difficulty];
}
std::array<int32_t, 5> CPUState::distribution(uint32_t count, uint32_t level,
                                              OriginalRandom &random) {
    validCount(count);
    const auto p = cpuParameters(level);
    const float ratio = random.range(p.ratioLower, p.ratioUpper);
    const float remainder = f32(1.0 - double(ratio));
    const float adjusted = ratio < 0.800000011920929 ? 0.8f : ratio;
    const float product = f32(double(adjusted) * double(remainder));
    const float weights[] = {f32(double(ratio) - product), product, product, 0.0f,
                             f32(double(remainder) - product)};
    std::array<int32_t, 5> counts{};
    int32_t sum = 0;
    for (size_t i = 0; i < counts.size(); ++i) {
        counts[i] = int32_t(double(weights[i]) * count);
        sum += counts[i];
    }
    for (int32_t i = int32_t(count) - sum; i > 0; --i)
        ++counts[(random.next() & 0x7fffffff) % 3];
    return counts;
}
void CPUState::initialize(uint32_t count, uint32_t level, OriginalRandom &random) {
    auto counts = distribution(count, level, random);
    std::vector<CPUEntry> prepared;
    prepared.reserve(count);
    for (int32_t judgment = 0; judgment < 5; ++judgment)
        for (int32_t i = 0; i < counts[size_t(judgment)]; ++i)
            prepared.push_back({judgment, 0, 0, 0});
    // 0x1015DCB0 skips its vector/shuffle path for count=0. It otherwise
    // seeds a separate stream once from the shared stream (0x10005610).
    if (count) {
        auto shuffle = OriginalRandom::seeded(random.next() & 0x7fffffff);
        for (size_t i = 1; i < prepared.size(); ++i) {
            const auto j = (shuffle.next() & 0x7fffffff) % (i + 1);
            std::swap(prepared[i], prepared[j]);
        }
    }
    difficulty = level;
    entries = std::move(prepared);
    assignOffsets(random);
}
void CPUState::assignOffsets(OriginalRandom &random) {
    for (auto &entry : entries) {
        const bool positive = random.range(0.0f, 100.0f) < 50.0;
        entry.tick = (entry.judgment == 1 || entry.judgment == 2) ? (positive ? 1 : -1) : 0;
    }
}
int32_t CPUState::reserveNext(int32_t judgment) {
    // lastIndex=-1 is a normal initial state. Other negative states would
    // dereference before the arrays in the original; reject those explicitly.
    if (lastIndex < -1 || lastIndex == INT32_MAX)
        throw std::invalid_argument("invalid CPU last index");
    int32_t index = lastIndex + 1;
    while (size_t(index) < entries.size()) {
        auto &entry = entries[size_t(index)];
        if (!entry.consumed && !entry.reserved) {
            entry.judgment = judgment;
            entry.reserved = 1;
            break;
        }
        ++index;
    }
    return index;
}
std::optional<CPUJudgment> CPUState::evaluate(const CPUNoteInput &note, OriginalRandom &random) {
    if (note.index < 0 || size_t(note.index) >= entries.size())
        return {};
    auto &entry = entries[size_t(note.index)];
    if (entry.judgment == 4)
        return {};
    const auto tick = note.kind == 4 && note.segmentIndex >= 0
                          ? sub32(note.segmentDuration, note.segmentElapsed)
                          : note.relativeTick;
    if (tick != entry.tick)
        return {};
    if (note.kind != 4 || !entry.consumed) {
        lastIndex = note.index;
        entry.consumed = 1;
    }
    int32_t event = 11;
    if (note.kind != 1 && note.kind != 4 && entry.judgment == 0 && specialEnabled &&
        (specialOverride >= 0 || meter >= 100) && (note.flags & 0x20)) {
        const float percent =
            specialOverride >= 0 ? f32(specialOverride) : cpuParameters(difficulty).specialPercent;
        if (random.chance(percent))
            event = 12;
    }
    return CPUJudgment{entry.judgment, event};
}
} // namespace rb
