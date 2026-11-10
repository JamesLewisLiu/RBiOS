#include "rb/StageLabelGraphic.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **argv) {
    try {
        for (int color = 0; color < 2; ++color) {
            StageLabelGraphicOptions options;
            options.localColor = color;
            const auto draws = drawStageLabels(options);
            if (draws.size() != 2 || draws[0].x != 384 || draws[0].y != 582 ||
                draws[1].x != 384 || draws[1].y != 560 || draws[0].layer != 76 ||
                draws[1].layer != 76 || draws[0].anchorX != .5f || draws[0].anchorY != .5f)
                throw Error("original stage label placement");
            if (draws[0].name != (color ? "score_you_blue" : "score_you_red") ||
                draws[1].name != (color ? "score_rival_red" : "score_rival_blue"))
                throw Error("original stage label color selection");
            if (argc == 2 && std::string(argv[1]) == "--describe") {
                for (const auto &d : draws)
                    std::cout << color << ' ' << d.name << ' ' << d.x << ' ' << d.y << ' '
                              << d.layer << ' ' << d.anchorX << ' ' << d.anchorY << '\n';
            } else if (argc == 2) {
                AfpPackage package(readFile(argv[1]));
                for (const auto &d : draws) {
                    const auto bitmap = package.bitmap(d.name);
                    if (bitmap.width != 124 || bitmap.height != 14)
                        throw Error("original stage label dimensions");
                }
            }
        }
        StageLabelGraphicOptions invalid;
        invalid.localColor = 2;
        bool rejected = false;
        try { drawStageLabels(invalid); } catch (const std::invalid_argument &) { rejected = true; }
        if (!rejected) throw Error("invalid stage label color accepted");
        if (argc != 2 || std::string(argv[1]) != "--describe")
            std::cout << "PASS StageLabelGraphic positions, colors, anchors and validation\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
