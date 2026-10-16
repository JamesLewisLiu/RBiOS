#include "rb/TouchInput.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **) {
    TouchInput in;
    if (argc > 1) {
        char op;
        while (std::cin >> op) {
            if (op == 'R') {
                in.reset();
                std::cin >> in.nextId;
            } else if (op == 'L') {
                size_t i;
                std::cin >> i;
                auto &c = in.state.contacts.at(i);
                std::cin >> c.reuseDelay >> c.selected >> c.captured >> c.blocked;
            } else {
                size_t n;
                std::cin >> n;
                std::vector<TouchPoint> points(n);
                for (auto &p : points)
                    std::cin >> p.x >> p.y;
                in.advance(points);
                std::cout << in.nextId << ' ';
                for (size_t i = 0; i < 15; ++i) {
                    const auto &c = in.state.contacts[i];
                    const auto &h = in.history[i];
                    uint32_t bits;
                    std::memcpy(&bits, &c.reuseDelay, 4);
                    std::cout << c.active << ' ' << c.id << ' ' << h.initial.x << ' ' << h.initial.y
                              << ' ' << h.previous.x << ' ' << h.previous.y << ' ' << c.x << ' '
                              << c.y << ' ' << unsigned(c.stateBits) << ' ' << h.heldFrames << ' '
                              << h.marker << ' ' << h.stationaryFrames << ' ' << h.age << ' '
                              << h.moved << ' ' << c.selected << ' ' << c.captured << ' '
                              << h.updated << ' ' << c.blocked << ' ' << bits << ' ';
                }
                std::cout << '\n';
            }
        }
        return 0;
    }
    in.advance({{200, 1100}});
    if (in.state.contacts[0].stateBits != 1)
        return 1;
    in.advance({{200, 1100}});
    if (in.state.contacts[0].stateBits != 2)
        return 2;
    in.advance({});
    if (in.state.contacts[0].stateBits != 4)
        return 3;
    in.advance({});
    if (in.state.contacts[0].active)
        return 4;
    in.nextId = INT32_MAX;
    in.advance({{100, 1000}});
    if (in.nextId != 0)
        return 5;
    std::cout << "TouchInput 5 checks passed\n";
}
