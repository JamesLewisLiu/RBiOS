#include "rb/Motion.hpp"
#include <iostream>
using namespace rb;
// Also provides a batched CLI for original-instruction differential validation.
int main(int argc, char **) {
    try {
        FieldGeometry f;
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                unsigned i;
                int x, y, s, w;
                float vx, vy;
                if (op == 'V') {
                    MotionRoute route;
                    int ticks, steps, pause;
                    uint32_t reflections, count;
                    std::cin >> ticks >> route.intermediateCount >> reflections >> count;
                    for (auto &p : route.points)
                        std::cin >> p.x >> p.y;
                    for (uint32_t k = 0; k < count; ++k) {
                        uint32_t p;
                        std::cin >> p;
                        route.order.push_back(p);
                    }
                    bool suppress;
                    std::cin >> steps >> pause >> suppress;
                    auto t = MotionTimeline::initialize(route, ticks, reflections);
                    t.pauseTicks = pause;
                    t.paused = pause > 0;
                    auto bits = [](float v) {
                        uint32_t b;
                        std::memcpy(&b, &v, 4);
                        std::cout << b << ' ';
                    };
                    bits(t.speed);
                    std::cout << t.segments.size() << ' ';
                    for (auto seg : t.segments) {
                        bits(seg.velocity.x);
                        bits(seg.velocity.y);
                        bits(seg.begin);
                        bits(seg.end);
                    }
                    for (auto v : t.reflectionTimes)
                        bits(v);
                    for (auto v : t.reflectionDirections)
                        std::cout << int(v) << ' ';
                    bits(t.specialReflectionTime);
                    std::cout << t.specialReflectionDirection << ' ';
                    MotionPosition position;
                    position.position = {float(route.points[0].x), float(route.points[0].y)};
                    position.rounded = route.points[0];
                    position.suppressOnce = suppress;
                    for (int step = 0; step < steps; ++step) {
                        position.integrate(t);
                        t.advance();
                    }
                    bits(t.velocity.x);
                    bits(t.velocity.y);
                    auto effective = t.effectiveVelocity();
                    bits(effective.x);
                    bits(effective.y);
                    std::cout << t.index << ' ' << t.previousIndex << ' ' << t.tick << ' '
                              << t.paused << ' ' << t.pauseTicks << ' ';
                    bits(position.position.x);
                    bits(position.position.y);
                    bits(position.delta.x);
                    bits(position.delta.y);
                    bits(position.previousDelta.x);
                    bits(position.previousDelta.y);
                    std::cout << position.rounded.x << ' ' << position.rounded.y << ' '
                              << position.suppressOnce << '\n';
                } else if (op == 'X' || op == 'Y') {
                    std::cin >> i;
                    std::cout << (op == 'X' ? f.x(i) : f.y(i)) << '\n';
                } else if (op == 'D') {
                    std::cin >> w >> i >> s;
                    auto p = f.destination(w, i, s);
                    std::cout << p.x << ' ' << p.y << '\n';
                } else if (op == 'L') {
                    std::cin >> i >> s;
                    std::cout << f.laneX(i) << ' ' << f.laneY(i, s) << '\n';
                } else if (op == 'R') {
                    std::cin >> x >> s;
                    std::cout << f.region(x, s) << '\n';
                } else if (op == 'B') {
                    std::cin >> x >> y >> vx >> vy;
                    auto h = f.firstBoundary({x, y}, vx, vy);
                    std::cout << int(h.boundary) << ' ' << h.point.x << ' ' << h.point.y << '\n';
                } else
                    throw Error("unknown motion command");
                if (!std::cin)
                    throw Error("incomplete motion command");
            }
            return 0;
        }
        auto check = [](bool ok) {
            if (!ok)
                throw Error("motion assertion");
        };
        check(f.x(3) == 384 && f.y(0) == 1222 && f.y(1) == 80);
        auto h = f.firstBoundary({384, 651}, 1, 0);
        check(h.boundary == Boundary::right && h.point.x == 732 && h.point.y == 651);
        h = f.firstBoundary({384, 651}, 0, -1);
        check(h.boundary == Boundary::top && h.point.y == 116);
        check(f.firstBoundary({384, 651}, 0, 0).boundary == Boundary::none);
        check(f.destination(1, 9, 0).x == 32 && f.destination(1, 9, 1).x == 736);
        check(f.region(256, 0) == 1 && f.region(512, 1) == 5);
        MotionRoute route;
        route.points[0] = {384, 204};
        route.points[4] = {384, 1098};
        route.points[6] = {384, 1186};
        route.order = {0, 4, 6};
        auto timeline = MotionTimeline::initialize(route, 60, 0);
        check(timeline.segments.size() == 2 && timeline.velocity.x == 0 &&
              timeline.velocity.y > 14);
        MotionPosition position;
        position.position = {384, 204};
        position.rounded = {384, 204};
        timeline.paused = true;
        timeline.pauseTicks = 3;
        for (int i = 0; i < 3; ++i) {
            position.integrate(timeline);
            timeline.advance();
        }
        check(position.rounded.y == 204 && timeline.tick == 0 && !timeline.paused);
        for (int i = 0; i < 63; ++i) {
            position.integrate(timeline);
            timeline.advance();
        }
        check(timeline.index == 1 && timeline.previousIndex == 1 && timeline.tick == 63 &&
              position.rounded.y > 1098);
        position.suppressOnce = true;
        const auto old = position.rounded;
        position.integrate(timeline);
        check(!position.suppressOnce && position.rounded.x == old.x && position.rounded.y == old.y);
        route.intermediateCount = 1;
        route.points[1] = {36, 651};
        route.order = {0, 1, 4, 6};
        timeline = MotionTimeline::initialize(route, 60, 1);
        check(timeline.reflectionDirections[0] == Boundary::left &&
              timeline.reflectionTimes[0] > 0);
        std::cout << "PASS 11 motion geometry/timeline assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
