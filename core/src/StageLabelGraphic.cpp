#include "rb/StageLabelGraphic.hpp"
#include <stdexcept>

namespace rb {
std::vector<SpriteDraw> drawStageLabels(const StageLabelGraphicOptions &options) {
    if (options.localColor < 0 || options.localColor > 1)
        throw std::invalid_argument("stage label color outside original range");
    std::vector<SpriteDraw> out;
    for (unsigned side = 0; side < 2; ++side) {
        const std::string player = side ? "rival" : "you";
        const std::string color = (options.localColor ^ int(side)) ? "blue" : "red";
        out.push_back({"score_" + player + "_" + color, options.position[side][0],
                       options.position[side][1], 76});
    }
    return out;
}
}
