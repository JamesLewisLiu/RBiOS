#pragma once
#include "NoteGraphic.hpp"
#include <array>

namespace rb {
struct StageLabelGraphicOptions {
    int localColor = 0;
    // Original normal-layout AFP centers; future layout playback can supply others.
    std::array<std::array<int32_t, 2>, 2> position{{{384, 582}, {384, 560}}};
};
// Ordinary battle labels from gmparts, inheriting gmbase_00's original layer 76.
std::vector<SpriteDraw> drawStageLabels(const StageLabelGraphicOptions &);
}
