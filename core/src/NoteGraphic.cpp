#include "rb/NoteGraphic.hpp"

namespace rb {
NoteGraphic selectNoteGraphic(const NoteGraphicInput &in) {
    if (in.color < 0 || in.color > 1 || in.side < 0 || in.side > 1)
        throw Error("note graphic color/side outside original table");
    const auto color = in.color == 0 ? "red" : "blue";
    const auto normal = std::string("obj_normal_") + color;
    const auto top = std::string("obj_top_") + color;
    const auto gold = std::string("obj_gold_") + color;
    const auto longHead = std::string("long_head_") + color;
    const auto longTop = std::string("long_head_top_") + color;
    const auto longBack = std::string("long_head_bg_") + color;
    const bool alternate = (in.presentationFlags & 8) && in.top == 1;
    NoteGraphic out;
    out.side = in.side;
    if (in.top == 1) {
        if (in.kind != 1 || in.side) {
            out.head = alternate ? normal : top;
            out.style = 3;
        } else {
            out.head = alternate ? longHead : longTop;
            out.background = alternate ? longBack : "long_head_bg_top";
            out.style = 4;
        }
    } else if (in.kind == 4) {
        out.style = 7;
        out.movieOn = "swich_on";
        out.movieOff = "swich_off";
    } else {
        // The original has separate in.destination<=9 / routeFlags&2 paths;
        // they converge on the same selection for every non-slide kind.
        if (in.kind == 1 && !in.side) {
            out.head = alternate ? longTop : longHead;
            out.background = alternate ? "long_head_bg_top" : longBack;
            out.style = 2;
        } else if (in.kind != 1 && in.kind != 3 && in.special > 0) {
            out.head = gold;
            out.style = 0;
        } else if (in.kind != 3 || in.side) {
            out.head = alternate ? top : normal;
            out.style = 1;
        } else {
            out.style = in.widthMode == 0 ? 5 : 6;
            out.movieOn =
                std::string(in.widthMode == 0 ? "obj_bigbang_big_" : "obj_bigbang_small_") + color;
        }
    }
    if (in.kind != 3) {
        out.specialOverlay = (in.flags & 1) != 0;
        out.secondaryOverlay = (in.flags & 2) != 0;
    }
    return out;
}
float noteGraphicAngle(const NoteGraphic &graphic, float direction) {
    switch (graphic.style) {
    case 0:
    case 1:
    case 5:
    case 6:
        return graphic.side == 1 ? 3.1415927f : 0.f;
    default:
        return direction;
    }
}
NoteGraphicDraws drawNoteGraphic(const NoteGraphic &g, int32_t x, int32_t y, float direction,
                                 float scale, float alpha, float appearanceScale, int32_t layer,
                                 bool switchHeld) {
    NoteGraphicDraws out;
    out.needsSpecialOverlay = g.specialOverlay;
    if (!g.movieOn.empty()) {
        out.movie = g.style == 8 ? g.movieOff : g.movieOn;
        if (g.style == 7 || g.style == 8)
            out.movieLabel = switchHeld ? "on" : "off";
        out.movieX = x; out.movieY = y; out.movieLayer = layer;
        out.movieAngle = noteGraphicAngle(g,direction);
        out.movieScale = scale * ((g.style == 5 || g.style == 6) && appearanceScale >= 1.f ? 1.f : appearanceScale);
        out.movieAlpha = alpha;
        return out;
    }
    const float angle = noteGraphicAngle(g, direction);
    const float combinedScale = scale * appearanceScale;
    if (!g.head.empty())
        out.sprites.push_back({g.head, x, y, layer, angle, combinedScale, alpha, .5f, .5f, 0});
    if (!g.background.empty())
        out.sprites.push_back({g.background, x, y, 61, angle, combinedScale, alpha, .5f, .5f, 8});
    return out;
}
} // namespace rb
