#include "rb/NoteGraphic.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--angles") {
            NoteGraphic g;
            uint32_t bits;
            while (std::cin >> g.style >> g.side >> bits) {
                float direction;
                std::memcpy(&direction, &bits, 4);
                const float angle = noteGraphicAngle(g, direction);
                std::memcpy(&bits, &angle, 4);
                std::cout << bits << '\n';
            }
            return 0;
        }
        if (argc > 1) {
            NoteGraphicInput v;
            unsigned presentation;
            while (std::cin >> v.color >> v.top >> v.kind >> v.side >> v.special >> v.destination >>
                   v.routeFlags >> v.widthMode >> v.flags >> presentation) {
                v.presentationFlags = static_cast<uint8_t>(presentation);
                const auto g = selectNoteGraphic(v);
                auto value = [](const std::string &s) { return s.empty() ? "-" : s; };
                std::cout << g.style << ' ' << value(g.head) << ' ' << value(g.background) << ' '
                          << value(g.movieOn) << ' ' << value(g.movieOff) << ' ' << g.specialOverlay
                          << ' ' << g.secondaryOverlay << '\n';
            }
            return 0;
        }
        size_t checks = 0;
        auto expect = [&](bool v) {
            if (!v)
                throw Error("note graphic check");
            ++checks;
        };
        auto g = selectNoteGraphic({});
        expect(g.head == "obj_normal_red");
        auto draws = drawNoteGraphic(g, 123, 456, 2.f, 1.2f, .8f, .5f);
        expect(draws.sprites.size() == 1 && draws.movie.empty());
        expect(draws.sprites[0].x == 123 && draws.sprites[0].y == 456);
        expect(draws.sprites[0].scale == .6f && draws.sprites[0].alpha == .8f);
        expect(draws.sprites[0].angle == 0 && draws.sprites[0].anchorX == .5f);
        NoteGraphicInput v;
        v.color = 1;
        v.kind = 1;
        g = selectNoteGraphic(v);
        expect(g.head == "long_head_blue" && g.background == "long_head_bg_blue");
        draws = drawNoteGraphic(g, 0, 0, 1.5f, 1, 1, 1);
        expect(draws.sprites.size() == 2 && draws.sprites[1].layer == 61);
        expect(draws.sprites[0].angle == 1.5f);
        v.kind = 3;
        g = selectNoteGraphic(v);
        draws = drawNoteGraphic(g, 0, 0, 0, 1, 1, 1);
        expect(draws.sprites.empty() && draws.movie == "obj_bigbang_big_blue");
        v.kind = 4;
        g = selectNoteGraphic(v);
        expect(drawNoteGraphic(g, 0, 0, 0, 1, 1, 1).movie == "swich_on");
        expect(drawNoteGraphic(g, 0, 0, 0, 1, 1, 1, 40, true).movie == "swich_on");
        v.kind = 0;
        v.side = 1;
        g = selectNoteGraphic(v);
        expect(noteGraphicAngle(g, 1.f) == 3.1415927f);
        std::cout << "PASS NoteGraphic " << checks << " checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
