#include "rb/ScoreDisplay.hpp"

namespace rb {
void updateScoreDisplay(BattleScore &score) {
    for (unsigned side = 0; side < 2; ++side) {
        auto &displayed = score.displayedScore[side];
        const auto actual = score.score[side];
        if (!score.animateScore[side]) {
            displayed = actual;
            continue;
        }
        if (displayed < actual)
            ++displayed;
        else if (displayed > actual)
            --displayed;
        if (displayed == actual)
            score.animateScore[side] = false;
    }
}
}
