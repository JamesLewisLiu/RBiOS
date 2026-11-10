#include "rb/ScoreDisplay.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--wire") {
            BattleScore b;
            int a, c;
            while (std::cin >> b.score[0] >> b.score[1] >> b.displayedScore[0] >>
                   b.displayedScore[1] >> a >> c) {
                b.animateScore = {{a != 0, c != 0}};
                updateScoreDisplay(b);
                std::cout << b.displayedScore[0] << ' ' << b.displayedScore[1] << ' '
                          << b.animateScore[0] << ' ' << b.animateScore[1] << '\n';
            }
            return 0;
        }
        auto check = [](bool ok) { if (!ok) throw std::runtime_error("score display regression"); };
        BattleScore b;
        b.score = {{100, 10}}; b.displayedScore = {{0, 50}};
        updateScoreDisplay(b);
        check(b.displayedScore == b.score);
        b.score = {{103, 7}}; b.animateScore = {{true, true}};
        updateScoreDisplay(b);
        check(b.displayedScore == std::array<int32_t, 2>{{101, 9}} && b.animateScore[0] && b.animateScore[1]);
        updateScoreDisplay(b); updateScoreDisplay(b);
        check(b.displayedScore == b.score && !b.animateScore[0] && !b.animateScore[1]);
        b.animateScore = {{true, true}}; updateScoreDisplay(b);
        check(!b.animateScore[0] && !b.animateScore[1]);
        b.score = {{std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min()}};
        b.displayedScore = {{b.score[0] - 1, b.score[1] + 1}}; b.animateScore = {{true, true}};
        updateScoreDisplay(b);
        check(b.displayedScore == b.score && !b.animateScore[0] && !b.animateScore[1]);
        std::cout << "PASS ScoreDisplay copy, step, stop and signed-boundary checks\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
