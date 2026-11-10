#pragma once
#include "Score.hpp"

namespace rb {
// Original 0x10155460, called once by the score manager's update (0x10164FC0).
// Changes only the two displayed totals and their animation flags.
void updateScoreDisplay(BattleScore &);
}
