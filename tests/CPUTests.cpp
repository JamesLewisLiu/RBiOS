#include "rb/CPU.hpp"
#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
using namespace rb;
static int checks = 0;
static void check(bool value) {
    ++checks;
    if (!value)
        throw std::runtime_error("CPU check " + std::to_string(checks));
}
static void inputRandom(OriginalRandom &r) {
    for (auto &v : r.words)
        std::cin >> v;
}
static void outputRandom(const OriginalRandom &r) {
    for (auto v : r.words)
        std::cout << ' ' << v;
}
static uint32_t bits(float f) {
    uint32_t result;
    std::memcpy(&result, &f, 4);
    return result;
}
static float real(uint32_t value) {
    float f;
    std::memcpy(&f, &value, 4);
    return f;
}
static void batch() {
    char command;
    while (std::cin >> command) {
        OriginalRandom random{};
        CPUState cpu;
        uint32_t count, level;
        if (command == 'p') {
            std::cin >> level;
            const auto p = cpuParameters(level);
            std::cout << bits(p.ratioLower) << ' ' << bits(p.ratioUpper) << ' '
                      << bits(p.specialPercent);
        } else if (command == 's') {
            std::cin >> count;
            random = OriginalRandom::seeded(count);
            std::cout << 0;
            outputRandom(random);
        } else if (command == 'r') {
            uint32_t lower, upper;
            std::cin >> lower >> upper;
            inputRandom(random);
            std::cout << bits(random.range(real(lower), real(upper)));
            outputRandom(random);
        } else if (command == 'd' || command == 'i') {
            std::cin >> count >> level;
            inputRandom(random);
            if (command == 'd') {
                auto d = CPUState::distribution(count, level, random);
                std::cout << d[0];
                for (size_t i = 1; i < d.size(); ++i)
                    std::cout << ' ' << d[i];
            } else {
                cpu.initialize(count, level, random);
                std::cout << cpu.lastIndex;
                for (const auto &v : cpu.entries)
                    std::cout << ' ' << v.judgment << ' ' << v.tick << ' ' << int(v.consumed) << ' '
                              << int(v.reserved);
            }
            outputRandom(random);
        } else if (command == 'e' || command == 'n') {
            std::cin >> count >> cpu.difficulty >> cpu.lastIndex >> cpu.specialEnabled >>
                cpu.meter >> cpu.specialOverride;
            inputRandom(random);
            cpu.entries.resize(count);
            for (auto &entry : cpu.entries) {
                int consumed, reserved;
                std::cin >> entry.judgment >> entry.tick >> consumed >> reserved;
                entry.consumed = uint8_t(consumed);
                entry.reserved = uint8_t(reserved);
            }
            if (command == 'e') {
                CPUNoteInput n;
                std::cin >> n.index >> n.kind >> n.flags >> n.relativeTick >> n.segmentIndex >>
                    n.segmentDuration >> n.segmentElapsed;
                const auto result = cpu.evaluate(n, random);
                std::cout << bool(result) << ' ' << (result ? result->judgment : 123456) << ' '
                          << (result ? result->event : 654321);
            } else {
                int32_t judgment;
                std::cin >> judgment;
                std::cout << cpu.reserveNext(judgment);
            }
            std::cout << ' ' << cpu.lastIndex;
            for (const auto &v : cpu.entries)
                std::cout << ' ' << v.judgment << ' ' << v.tick << ' ' << int(v.consumed) << ' '
                          << int(v.reserved);
            outputRandom(random);
        } else {
            throw std::runtime_error("unknown CPU batch command");
        }
        if (!std::cin)
            throw std::runtime_error("invalid CPU batch input");
        std::cout << '\n';
    }
}
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--batch") {
            batch();
            return 0;
        }
        auto random = OriginalRandom::seeded(42);
        for (uint32_t level = 0; level < 19; ++level) {
            CPUState cpu;
            cpu.lastIndex = 7;
            cpu.initialize(91, level, random);
            check(cpu.entries.size() == 91 && cpu.lastIndex == 7);
            int32_t total = 0;
            for (const auto &v : cpu.entries) {
                check(v.judgment >= 0 && v.judgment <= 4);
                check(v.judgment == 1 || v.judgment == 2 ? (v.tick == -1 || v.tick == 1)
                                                         : v.tick == 0);
                check(v.tick >= -1 && v.tick <= 1 && !v.consumed && !v.reserved);
                ++total;
            }
            check(total == 91);
        }
        CPUState cpu;
        check(cpu.specialEnabled && cpu.lastIndex == -1 && cpu.specialOverride == -1);
        cpu.entries = {{0, 0, 0, 0}, {1, 1, 0, 0}, {2, -1, 0, 0}, {4, 0, 0, 0}};
        CPUNoteInput note;
        check(!cpu.evaluate(note, random));
        note.index = 0;
        auto result = cpu.evaluate(note, random);
        check(result && result->judgment == 0 && result->event == 11);
        check(cpu.lastIndex == 0 && cpu.entries[0].consumed == 1);
        note.index = 3;
        check(!cpu.evaluate(note, random));
        note.index = 1;
        check(!cpu.evaluate(note, random));
        note.relativeTick = 1;
        check(cpu.evaluate(note, random).has_value());
        check(cpu.reserveNext(4) == 2 && cpu.entries[2].reserved == 1);
        check(cpu.reserveNext(0) == 3 && cpu.entries[3].reserved == 1);
        check(cpu.reserveNext(0) == 4);
        cpu.specialEnabled = true;
        cpu.specialOverride = 101;
        note.index = 0;
        note.relativeTick = 0;
        note.flags = 0x20;
        result = cpu.evaluate(note, random);
        check(result && result->event == 12);
        note.kind = 1;
        result = cpu.evaluate(note, random);
        check(result && result->event == 11);
        note.kind = 4;
        note.segmentIndex = 0;
        note.segmentDuration = 17;
        note.segmentElapsed = 17;
        cpu.lastIndex = 2;
        check(cpu.evaluate(note, random).has_value() && cpu.lastIndex == 2);
        check(OriginalRandom{{0, 0, 0, 0}}.next() == 0);
        std::cout << "PASS " << checks << " CPU checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
