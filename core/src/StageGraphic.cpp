#include "rb/StageGraphic.hpp"
#include <cmath>

namespace rb {
AfpSampleOptions stageFrameOptions(unsigned difficulty, uint32_t elapsed, unsigned round,
                                    int localColor) {
    if (difficulty >= 4 || round >= 7 || localColor < 0 || localColor > 1)
        throw Error("invalid original frame selection");
    AfpSampleOptions options;
    options.playbackTicks = elapsed;
    for (const auto *side : {"you", "rival"})
        for (const auto *field : {"name", "torina", "shop", "area", "characard_s"})
            options.hiddenObjects.insert(std::string(field) + "_" + side);
    const char *difficulties[] = {"b", "m", "h", "wh"};
    options.textureOverrides["frame_round"] = "frame_round_0" + std::to_string(round);
    options.textureOverrides["frame_dif"] = std::string("frame_dif_") + difficulties[difficulty];
    options.textureOverrides["player_you"] = localColor ? "player_you_blue" : "player_you_red";
    options.textureOverrides["player_rival"] = localColor ? "player_rival_red" : "player_rival_blue";
    options.textureOverrides["orb_you"] = localColor ? "orb_blue" : "orb_red";
    options.textureOverrides["orb_rival"] = localColor ? "orb_red" : "orb_blue";
    return options;
}
std::vector<SpriteDraw> drawStageJudgmentGraphics(const StageGraphicOptions &o) {
    if (o.localColor < 0 || o.localColor > 1 || o.topLaneCount > 3)
        throw Error("invalid original stage graphic selection");
    for (float a : {o.lineAlpha, o.lineBackgroundAlpha, o.topAlpha, o.topPulse})
        if (!std::isfinite(a) || a < 0 || a > 1)
            throw Error("invalid stage graphic alpha");
    std::vector<SpriteDraw> out;
    for (int side = 0; side < 2; ++side) {
        const auto color = std::string((side ? o.localColor == 0 : o.localColor) ? "blue" : "red");
        const auto y = o.field.y(side ? 5 : 4);
        out.push_back({"line_normal_" + color, 384, y, 69, 0, 1, o.lineAlpha, .5f, .5f, 0});
        out.push_back(
            {"line_normal_" + color + "_bg", 384, y, 70, 0, 1, o.lineBackgroundAlpha, .5f, .5f, 8});
        if (side && !o.opponentTopVisible)
            continue;
        for (unsigned lane = 0; lane < o.topLaneCount; ++lane) {
            const auto x = o.field.x(lane == 0 ? 2 : lane == 1 ? 4 : 3);
            const auto topY = o.field.y(side ? 7 : 6);
            out.push_back({"line_top_parts", x, topY, 67, 0, 1, o.topAlpha, .5f, .5f, 8});
            out.push_back({"line_top_" + color + "_mark_" + (side ? "rival" : "you"), x, topY, 69,
                           0, 1, o.topAlpha * o.topPulse * .5f, .5f, .5f, 8});
            out.push_back({"line_top_" + color, x, topY, 70, 0, 1, o.topAlpha, .5f, .5f, 0});
        }
    }
    return out;
}
} // namespace rb
