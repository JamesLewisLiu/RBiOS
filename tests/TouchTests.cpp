#include "rb/Binary.hpp"
#include "rb/Touch.hpp"
#include <cstring>
#include <iostream>
using namespace rb;
namespace {
std::vector<uint8_t> raw(1976);
int32_t i(unsigned o) {
    int32_t v;
    std::memcpy(&v, raw.data() + o, 4);
    return v;
}
float f(unsigned o) {
    float v;
    std::memcpy(&v, raw.data() + o, 4);
    return v;
}
void put(unsigned o, int32_t v) {
    std::memcpy(raw.data() + o, &v, 4);
}
void fp(unsigned o, float v) {
    std::memcpy(raw.data() + o, &v, 4);
}
void load(NoteTouchState &n, TouchState &t) {
    std::string s;
    std::cin >> s;
    if (s.size() != raw.size() * 2)
        throw Error("touch fixture size");
    for (unsigned k = 0; k < raw.size(); ++k)
        raw[k] = uint8_t(std::stoul(s.substr(k * 2, 2), nullptr, 16));
    n.kind = i(448 + 288);
    n.relativeTick = i(192);
    n.durationTicks = i(296);
    n.segmentElapsed = i(360);
    n.segmentIndex = i(364);
    n.segmentDuration = i(368);
    n.ended = raw[376];
    n.x = i(100);
    n.y = i(104);
    n.phase = i(300);
    n.grace = i(304);
    n.side = i(448 + 328);
    n.region = i(448 + 284);
    n.touchId = i(216);
    n.segmentReverse = i(372);
    n.cooldown = i(420);
    n.windowStart = i(392);
    n.windowEnd = i(396);
    n.switchWindowStart = i(400);
    n.boundaryY = i(384);
    n.extendedBottom = i(388);
    n.initialTiming = i(448 + 16);
    n.widthMode = i(448 + 400);
    n.noteMode = i(312);
    n.wide = raw[448 + 357];
    n.inputBlocked = raw[411];
    n.missFlag = raw[408];
    n.lastPress = {i(428), i(432)};
    for (unsigned k = 0; k < 15; ++k) {
        unsigned o = 864 + 64 * k;
        auto &c = t.contacts[k];
        c.active = raw[o];
        c.id = i(o + 4);
        c.x = i(o + 24);
        c.y = i(o + 28);
        c.stateBits = raw[o + 32];
        c.selected = raw[o + 53];
        c.captured = raw[o + 54];
        c.blocked = raw[o + 56];
        c.reuseDelay = f(o + 60);
    }
    t.lastSelectedId = i(1824);
    for (int k = 0; k < i(1844); ++k)
        t.supplemental.push_back({i(1848 + 8 * k), i(1852 + 8 * k)});
}
void save(NoteTouchState &n, TouchState &t) {
    put(304, n.grace);
    put(216, n.touchId);
    put(428, n.lastPress.x);
    put(432, n.lastPress.y);
    put(1824, t.lastSelectedId);
    for (unsigned k = 0; k < 15; ++k) {
        unsigned o = 864 + 64 * k;
        auto &c = t.contacts[k];
        raw[o + 53] = c.selected;
        raw[o + 54] = c.captured;
        fp(o + 60, c.reuseDelay);
    }
    constexpr char h[] = "0123456789abcdef";
    for (auto c : raw)
        std::cout << h[c >> 4] << h[c & 15];
    std::cout << '\n';
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                NoteTouchState n;
                TouchState t;
                load(n, t);
                bool yes = false;
                int reason = -77;
                if (op == 'H')
                    yes = n.maintainHold(t, f(1828), i(1832));
                else if (op == 'V')
                    yes = n.maintainSwitch(t, f(1828), i(1832));
                else if (op == 'S')
                    yes = n.switchPress(t, f(1828), reason);
                else if (op == 'F')
                    yes = n.firstPress(t, f(1828));
                else if (op == 'M')
                    yes = n.missed(f(1828), i(1836), [&](int, int) { return i(1840) != 0; });
                else if (op == 'J')
                    yes = n.missed(f(1828), i(1836), [&](int d, int m) {
                        return n.inField(FieldGeometry{}, d, m, f(1828));
                    });
                else if (op == 'G') {
                    int d, m;
                    std::cin >> d >> m;
                    yes = n.inField(FieldGeometry{}, d, m, f(1828));
                } else if (op == 'W')
                    yes = n.touchWindow();
                else if (op == 'X')
                    yes = n.extendedWindow();
                else if (op == 'B' || op == 'P' || op == 'U') {
                    TouchRect r;
                    std::cin >> r.x >> r.y >> r.width >> r.height;
                    yes = op == 'B'   ? t.held(r)
                          : op == 'U' ? t.supplementary(r)
                                      : t.press(r, true, true, &n.lastPress);
                } else
                    throw Error("touch fixture opcode");
                std::cout << yes << ' ' << reason << ' ';
                save(n, t);
            }
            return 0;
        }
        unsigned checks = 0;
        auto check = [&](bool b) {
            ++checks;
            if (!b)
                throw Error("touch assertion " + std::to_string(checks));
        };
        TouchState t;
        t.contacts[0] = {true, 1, 10, 10, 3};
        t.contacts[1] = {true, 2, 11, 10, 3};
        check(!t.held({11, 0, 10, 20}));
        check(t.held({0, 0, 20, 20}) && t.lastSelectedId == 1);
        check(t.press({0, 0, 20, 20}) && t.lastSelectedId == 1);
        check(t.press({0, 0, 20, 20}) && t.lastSelectedId == 2);
        check(!t.press({0, 0, 20, 20}));
        check(t.press({0, 0, 20, 20}, true, true) && t.contacts[0].captured);
        NoteTouchState n;
        n.kind = 1;
        n.phase = 3;
        n.x = 384;
        n.y = 1100;
        n.durationTicks = 100;
        n.relativeTick = -20;
        n.windowEnd = 9;
        TouchState empty;
        for (int k = 0; k < 5; ++k)
            check(n.maintainHold(empty, 1, false));
        check(!n.maintainHold(empty, 1, false));
        check(n.touchWindow());
        n.ended = true;
        check(!n.touchWindow());
        n.ended = false;
        n.relativeTick = -10;
        n.noteMode = 6;
        check(n.missed(1, false, {}));
        n.relativeTick = 1;
        check(!n.missed(1, false, {}));
        FieldGeometry field;
        check(fieldContains(field, 0, 0, 36, 0, 36));
        check(!fieldContains(field, 0, 0, 36, 0, 35));
        check(fieldContains(field, 1, 0, 36, 0, 732));
        check(!fieldContains(field, 1, 0, 36, 0, 733));
        check(fieldContains(field, 2, 116, 36, 0, 0));
        check(!fieldContains(field, 2, 115, 36, 0, 0));
        check(fieldContains(field, 3, 1186, 36, 0, 0));
        check(!fieldContains(field, 3, 1187, 36, 0, 0));
        check(!fieldContains(field, 4, 1186, 36, 0, 0));
        std::cout << "PASS " << checks << " touch assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
