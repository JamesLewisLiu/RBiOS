#include "rb/StageContinuousSound.hpp"
#include <iostream>
#include <stdexcept>
using namespace rb;
int main(int argc, char **) {
    try {
        if (argc > 1) {
            int current, desired;
            while (std::cin >> current >> desired) {
                StageContinuousSound s;
                s.beginFrame(); s.observe(0, 1, current != 0); s.endFrame();
                s.beginFrame(); s.observe(0, 4, desired != 0);
                std::cout << int(s.endFrame()) << ' ' << s.active() << '\n';
            }
            return 0;
        }
        unsigned checks = 0;
        auto expect = [&](bool b) { ++checks; if (!b) throw std::runtime_error("loop test " + std::to_string(checks)); };
        for (int side : {-1, 0, 1}) for (int kind = 0; kind <= 5; ++kind)
            for (bool held : {false, true}) for (bool enabled : {false, true}) {
                StageContinuousSound s; s.beginFrame(); s.observe(side, kind, held, enabled);
                bool wanted = side == 0 && (kind == 1 || kind == 4) && held && enabled;
                expect(s.endFrame() == (wanted ? LoopChange::start : LoopChange::none));
                expect(s.active() == wanted);
            }
        StageContinuousSound s;
        s.beginFrame(); s.observe(0, 1, true); expect(s.endFrame() == LoopChange::start);
        s.beginFrame(); s.observe(0, 1, false); s.observe(0, 4, true); expect(s.endFrame() == LoopChange::none);
        s.beginFrame(); s.observe(0, 1, true); s.observe(0, 4, true); expect(s.endFrame() == LoopChange::none);
        s.beginFrame(); s.observe(1, 1, true); expect(s.endFrame() == LoopChange::stop);
        s.beginFrame(); expect(s.endFrame() == LoopChange::none);
        s.beginFrame(); s.observe(0, 4, true); expect(s.endFrame() == LoopChange::start);
        s.reset(); expect(!s.active()); s.beginFrame(); expect(s.endFrame() == LoopChange::none);
        expect(StageContinuousSound::resource().id == 14672);
        std::cout << "PASS " << checks << " continuous sound checks\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
