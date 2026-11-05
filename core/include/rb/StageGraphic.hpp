#pragma once
#include "Motion.hpp"
#include "NoteGraphic.hpp"
#include "AfpTimeline.hpp"

namespace rb {
struct StageGraphicOptions {
    FieldGeometry field;
    int32_t localColor = 0; // original profile +20, 0 red / 1 blue
    float lineAlpha = 1, lineBackgroundAlpha = 1;
    float topAlpha = 1, topPulse = 1; // CJudgeCircleChip::draw percentage / modulation
    unsigned topLaneCount = 2;        // original default two lanes; optional central third lane
    bool opponentTopVisible = true;
};
// Original CStageJudgeLine (1014F640/1014F550) and CJudgeCircleChip
// (1014FEC0/1014FC10), without their separate AFP effect movies.
std::vector<SpriteDraw> drawStageJudgmentGraphics(const StageGraphicOptions &);
AfpSampleOptions stageFrameOptions(unsigned difficulty, uint32_t elapsed, unsigned round = 0,
                                    int localColor = 0);
} // namespace rb
