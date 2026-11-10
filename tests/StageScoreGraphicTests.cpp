#include "rb/StageScoreGraphic.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char **argv) {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        rb::StageScoreGraphicOptions o;
        int y0, y1;
        while (std::cin >> o.score[0] >> o.score[1] >> o.combo[0] >> o.combo[1] >> y0 >> y1 >> o.localColor) {
            o.comboYellow = {{y0 != 0, y1 != 0}};
            const auto draws = rb::drawStageScoreGraphics(o);
            std::cout << draws.size() << '\n';
            for (const auto &d : draws)
                std::cout << d.name << ' ' << d.x << ' ' << d.y << ' '
                          << d.anchorX << ' ' << d.anchorY << ' ' << d.layer << '\n';
        }
        return 0;
    }
    auto check = [](bool value) { if (!value) throw std::runtime_error("stage score draw regression"); };
    rb::StageScoreGraphicOptions o;
    auto draws = rb::drawStageScoreGraphics(o);
    check(draws.size() == 6 && draws[0].name == "score_as_small_red_00");
    o.score = {{1234, 10}}; o.combo = {{100, 99}}; o.comboYellow = {{false, true}};
    draws = rb::drawStageScoreGraphics(o);
    check(draws[0].name == "score_as_small_red_04" && draws[0].x == 450);
    check(draws[3].name == "score_as_small_red_01" && draws[3].x == 318);
    check(draws[4].name == "score_as_small_blue_00" && draws[4].x == 406);
    check(draws[6].name == "combo_big_as_white_00" && draws[6].x == 468);
    check(draws[10].name == "combo_small_as_yellow_09" && draws[10].x == 398);
    o.score[0] = 10000; draws = rb::drawStageScoreGraphics(o);
    check(draws[0].name == "score_as_small_red_00" && draws[3].name == "score_as_small_red_00");
    o.localColor = 1; draws = rb::drawStageScoreGraphics(o);
    check(draws[0].name == "score_as_small_blue_00");
    o.combo[0] = -1;
    bool rejected = false;
    try { rb::drawStageScoreGraphics(o); } catch (const std::invalid_argument &) { rejected = true; }
    check(rejected);
    std::cout << "PASS StageScoreGraphic zero, centering, truncation, colors and invalid input\n";
}
