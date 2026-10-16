#include "rb/Route.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            while (std::cin >> op) {
                if (op == 'J') {
                    RouteInput r;
                    ChartRouteConfig c;
                    MotionPoint start;
                    std::cin >> r.start.x >> r.start.y;
                    for (auto &n : r.intermediates)
                        std::cin >> n;
                    std::cin >> r.destination >> c.noteVariant >> c.side >> c.destination >>
                        c.direction >> c.overrideKind >> start.x >> start.y;
                    redirectRoute(r, c, start);
                    std::cout << r.start.x << ' ' << r.start.y << ' ';
                    for (auto n : r.intermediates)
                        std::cout << n << ' ';
                    std::cout << r.destination << ' ' << c.direction << '\n';
                } else if (op == 'C') {
                    ChartRouteInput in;
                    std::cin >> in.noteVariant >> in.side >> in.destination >> in.direction >>
                        in.pathKind >> in.parameter >> in.specialCounter >> in.previousTarget.x >>
                        in.previousTarget.y;
                    auto c = configureChartRoute(in);
                    std::cout << c.start.x << ' ' << c.start.y << ' ' << c.pathClass << ' '
                              << c.noteVariant << ' ' << c.side << ' ' << c.pathKind << ' '
                              << c.specialCounter << ' ' << c.parameter << ' ' << c.destination
                              << ' ' << c.direction << ' ' << c.overrideKind << '\n';
                } else if (op == 'S') {
                    RouteSeedConfig c;
                    std::cin >> c.start.x >> c.start.y >> c.pathClass >> c.side >> c.destination >>
                        c.direction;
                    auto r = initializeRoute(c);
                    std::cout << r.start.x << ' ' << r.start.y << ' ';
                    for (auto n : r.intermediates)
                        std::cout << n << ' ';
                    std::cout << r.destination << '\n';
                } else {
                    RouteInput in;
                    std::cin >> in.start.x >> in.start.y;
                    for (auto &n : in.intermediates)
                        std::cin >> n;
                    std::cin >> in.destination >> in.side >> in.kind >> in.suppressWallReflection >>
                        in.widthMode;
                    auto out = buildRoute(in);
                    for (auto p : out.route.points)
                        std::cout << p.x << ' ' << p.y << ' ';
                    std::cout << out.route.intermediateCount << ' ' << out.wallReflection << ' '
                              << out.route.order.size() - 1 << ' ';
                    for (unsigned i = 0; i < 7; ++i)
                        std::cout << (i < out.route.order.size() ? out.route.order[i] : 8) << ' ';
                    for (unsigned i = 0; i < 7; ++i) {
                        uint32_t bits;
                        std::memcpy(&bits, &out.cumulativeFractions[i], 4);
                        std::cout << bits << ' ';
                    }
                    std::cout << '\n';
                }
            }
            return 0;
        }
        RouteSeedConfig c;
        c.start = {384, 651};
        c.destination = 12;
        auto seed = initializeRoute(c);
        auto out = buildRoute(seed);
        if (out.route.order.front() != 0 || out.route.order.back() != 6 ||
            out.route.intermediateCount != 0)
            return 1;
        c.pathClass = 1;
        seed = initializeRoute(c);
        if (seed.intermediates[0] < 2 || seed.intermediates[0] > 5)
            return 2;
        std::cout << "Route 2 checks passed\n";
    } catch (const std::exception &e) {
        std::cerr << e.what();
        return 1;
    }
}
