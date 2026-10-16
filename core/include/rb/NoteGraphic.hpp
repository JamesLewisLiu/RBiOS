#pragma once
#include "AFP.hpp"

namespace rb {
struct NoteGraphicInput {
    int32_t color = 0;             // chart +280 (0 red / 1 blue)
    int32_t top = 0, kind = 0;     // chart +284 / +288
    int32_t side = 0;              // chart +328
    int32_t special = 0;           // chart +16
    uint32_t destination = 0;      // chart +352
    uint32_t routeFlags = 0;       // chart +372
    int32_t widthMode = 0;         // chart +400
    uint32_t flags = 0;            // chart +300
    uint8_t presentationFlags = 0; // 10155F10 argument
};
struct NoteGraphic {
    int32_t style = 0, side = 0;
    std::string head, background;
    std::string movieOn, movieOff; // AFP-only styles, no static replacement
    bool specialOverlay = false, secondaryOverlay = false;
};
NoteGraphic selectNoteGraphic(const NoteGraphicInput &);      // 0x10155F10
float noteGraphicAngle(const NoteGraphic &, float direction); // 0x10155800
struct SpriteDraw {
    std::string name;
    int32_t x = 0, y = 0, layer = 40;
    float angle = 0, scale = 1, alpha = 1;
    float anchorX = .5f, anchorY = .5f; // original sprite alignment (3,3)
    uint32_t afpBlend = 0;              // raw AFP param 4102: background passes 8
};
struct NoteGraphicDraws {
    std::vector<SpriteDraw> sprites;
    std::string movie;      // nonempty requires original AFP execution
    std::string movieLabel; // switch movie label "on"/"off", independent of resource
    int32_t movieX = 0, movieY = 0, movieLayer = 40;
    float movieAngle = 0, movieScale = 1, movieAlpha = 1;
    bool needsSpecialOverlay = false;
};
// The static-head portion of 0x10155A00. Original movie and JR overlay effects
// remain explicit rather than being replaced with invented pictures.
NoteGraphicDraws drawNoteGraphic(const NoteGraphic &, int32_t x, int32_t y, float direction,
                                 float scale, float alpha, float appearanceScale,
                                 int32_t layer = 40, bool switchHeld = false);
} // namespace rb
