#include "rb/StageGraphic.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **argv) {
    try {
        unsigned checks = 0;
        auto check = [&](bool ok) {
            if (!ok)
                throw Error("stage graphic check");
            ++checks;
        };
        StageGraphicOptions options;
        auto draws = drawStageJudgmentGraphics(options);
        check(draws.size() == 16);
        check(draws[0].name == "line_normal_red" && draws[0].x == 384 && draws[0].y == 1098 &&
              draws[0].layer == 69);
        check(draws[1].name == "line_normal_red_bg" && draws[1].afpBlend == 8 &&
              draws[1].layer == 70);
        check(draws[2].name == "line_top_parts" && draws[2].x == 128 && draws[2].y == 871);
        check(draws[3].name == "line_top_red_mark_you" && draws[3].alpha == .5f);
        check(draws[8].name == "line_normal_blue" && draws[8].y == 204);
        check(draws[10].x == 128 && draws[10].y == 431);
        options.localColor = 1;
        options.field.topOffset = 19;
        options.topLaneCount = 3;
        draws = drawStageJudgmentGraphics(options);
        check(draws.size() == 22 && draws[0].name == "line_normal_blue");
        check(draws[8].x == 384 && draws[8].y == 871);
        check(draws[11].name == "line_normal_red" && draws[11].y == 185);
        options.opponentTopVisible = false;
        draws = drawStageJudgmentGraphics(options);
        check(draws.size() == 13);
        options.localColor = 3;
        bool rejected = false;
        try {
            drawStageJudgmentGraphics(options);
        } catch (const Error &) {
            rejected = true;
        }
        check(rejected);
        if (argc == 2) {
            AfpPackage parts(readFile(argv[1]));
            for (int color = 0; color < 2; ++color) {
                options = {};
                options.localColor = color;
                options.topLaneCount = 3;
                for (const auto &d : drawStageJudgmentGraphics(options)) {
                    auto bitmap = parts.bitmap(d.name);
                    check(bitmap.width > 0 && bitmap.height > 0);
                }
            }
        }
        std::cout << "PASS StageGraphic " << checks << " checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
