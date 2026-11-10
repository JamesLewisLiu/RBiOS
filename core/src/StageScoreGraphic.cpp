#include "rb/StageScoreGraphic.hpp"
#include <stdexcept>

namespace rb {
namespace {
void digits(std::vector<SpriteDraw> &out, int32_t value, const std::string &prefix,
            std::array<int32_t, 2> position, int pitch) {
    // Original centering branches cap the offset at four digits, then emit LSD first.
    int offset = value >= 1000 ? 3 * pitch / 2 :
                 value >= 100 ? pitch : value >= 10 ? pitch / 2 : 0;
    int x = position[0] + offset;
    for (int i = 0; i < 4; ++i) {
        const int digit = value % 10;
        value /= 10;
        out.push_back({prefix + "0" + std::to_string(digit), x, position[1], 75});
        x -= pitch;
        if (value <= 0)
            break;
    }
}
}
std::vector<SpriteDraw> drawStageScoreGraphics(const StageScoreGraphicOptions &options) {
    if (options.localColor < 0 || options.localColor > 1)
        throw std::invalid_argument("stage score color outside original range");
    for (unsigned side = 0; side < 2; ++side)
        if (options.score[side] < 0 || options.combo[side] < 0)
            throw std::invalid_argument("stage score graphics require nonnegative values");
    std::vector<SpriteDraw> out;
    for (unsigned side = 0; side < 2; ++side) {
        const bool blue = (options.localColor ^ int(side)) != 0;
        digits(out, options.score[side], blue ? "score_as_small_blue_" : "score_as_small_red_",
               options.scorePosition[side], 44);
    }
    for (unsigned side = 0; side < 2; ++side) {
        const std::string size = side ? "small" : "big";
        const std::string color = options.comboYellow[side] ? "yellow" : "white";
        digits(out, options.combo[side], "combo_" + size + "_as_" + color + "_",
               options.comboPosition[side], side ? 28 : 84);
        const auto &position = options.comboLabelPosition[side];
        out.push_back({"combo_" + size + "_" + color, position[0], position[1], 75});
    }
    return out;
}
} // namespace rb
