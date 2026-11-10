#pragma once
#include "NoteGraphic.hpp"
#include <array>

namespace rb {
struct StageScoreGraphicOptions {
    // Values consumed by original CStageBattleScore::draw (10152600).
    std::array<int32_t, 2> score{}, combo{};
    // Local: miss count == 0. Rival: PlayerScore::fullCombo (+104).
    std::array<bool, 2> comboYellow{{true, true}};
    int localColor = 0;
    // Original gmparts/gmbase_00 normal placements. Animation may supply others.
    std::array<std::array<int32_t, 2>, 2> scorePosition{{{384, 624}, {384, 516}}};
    std::array<std::array<int32_t, 2>, 2> comboPosition{{{384, 737}, {384, 300}}};
    std::array<std::array<int32_t, 2>, 2> comboLabelPosition{{{384, 807}, {384, 269}}};
};
// Ordinary battle mode only. All returned textures belong to original gmparts.ifs.
// Input scores are the caller's displayed scores; this function does not advance them.
std::vector<SpriteDraw> drawStageScoreGraphics(const StageScoreGraphicOptions &);
} // namespace rb
