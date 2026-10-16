#include "rb/LongNoteGraphic.hpp"
#include <iostream>
using namespace rb;
static uint32_t bits(float v) {
    uint32_t b;
    std::memcpy(&b, &v, 4);
    return b;
}
static void print(const LongGraphicDraws &v) {
    std::cout << v.draws.size();
    for (const auto &d : v.draws)
        std::cout << ' ' << d.resource << ' ' << d.x << ' ' << d.y << ' ' << d.layer << ' '
                  << bits(d.angle) << ' ' << bits(d.scaleX) << ' ' << bits(d.scaleY) << ' '
                  << bits(d.alpha) << ' ' << bits(d.anchorY) << ' ' << d.afpBlend;
    std::cout << ' ' << v.overlays.size();
    for (const auto &d : v.overlays)
        std::cout << ' ' << d.category << ' ' << d.x << ' ' << d.y << ' ' << bits(d.scale) << ' '
                  << bits(d.alpha);
    std::cout << '\n';
}
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                if (op == 'L') {
                    LongNoteGraphicState s;
                    s.active = true;
                    int color, top, side, flags, mode;
                    MotionPoint head, tail;
                    MotionVector v;
                    float stage;
                    std::cin >> color >> top >> side >> flags >> mode >> head.x >> head.y >>
                        tail.x >> tail.y >> stage >> s.tailScale >> s.alpha >> v.x >> v.y;
                    if (mode)
                        updateLongNoteGraphicDirection(s, head, v);
                    else
                        updateLongNoteGraphic(s, head, tail, stage);
                    print(drawLongNoteGraphic(
                        selectLongNoteGraphic(color, top, side, uint8_t(flags)), s));
                } else if (op == 'S') {
                    SwitchLongGraphicState s;
                    int count;
                    std::cin >> s.active >> s.held >> s.color >> s.head.x >> s.head.y >>
                        s.trail.x >> s.trail.y >> s.velocity.x >> s.velocity.y >> s.trailTicks >>
                        s.secondaryTrailTicks >> s.clipY >> s.scale >> s.alpha >> count;
                    for (int i = 0; i < count; ++i) {
                        SwitchGraphicSegment p;
                        std::cin >> p.duration >> p.collection >> p.special >> p.secondarySpecial;
                        s.segments.push_back(p);
                    }
                    updateSwitchLongGraphic(s);
                    print(drawSwitchLongGraphic(s));
                } else
                    throw Error("unknown long graphic test command");
            }
            return 0;
        }
        unsigned checked = 0;
        const auto check = [&](bool value) {
            if (!value)
                throw Error("long graphic assertion");
            ++checked;
        };
        auto style = selectLongNoteGraphic(1, 1, 0);
        check(style.body == "long_body_top" && style.tailLayer == 39);
        check(selectLongNoteGraphic(1, 1, 0, 8).tail == "long_tail_blue");
        LongNoteGraphicState a;
        a.active = true;
        updateLongNoteGraphic(a, {100, 800}, {100, 488}, 1);
        check(a.bodyScaleY == 1 && std::abs(a.angle) < 1e-6f);
        const auto draws = drawLongNoteGraphic(style, a);
        check(draws.draws.size() == 3 && draws.draws[0].resource == "long_body_top" &&
              draws.draws[0].anchorY == 0 && draws.draws[0].layer == 62);
        SwitchLongGraphicState s;
        s.active = true;
        s.head = {100, 1000};
        s.trail = {100, 600};
        s.velocity = {0, 10};
        s.trailTicks = 1;
        s.clipY = 1200;
        s.segments = {{10, {}}, {10, {}}, {10, {}}};
        updateSwitchLongGraphic(s);
        check(s.segments[2].end.y == 700);
        auto sw = drawSwitchLongGraphic(s);
        check(sw.draws.size() == 7 && sw.draws.back().resource == "long_tail_bg_switch");
        check(sw.draws[0].resource == "swich_long_body_on");
        s.active = false;
        check(drawSwitchLongGraphic(s).draws.empty());
        std::cout << "PASS " << checked << " original long graphic assertions\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
