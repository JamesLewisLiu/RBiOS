#pragma once
#include "Motion.hpp"
#include <string>
#include <vector>

namespace rb {
// Non-uniform scale and alignment are required by the original 312-pixel body.
// Coordinates and clockwise angles remain in the original top-down field.
struct LongGraphicDraw {
    std::string resource, label;
    bool movie = false;
    int32_t x = 0, y = 0, layer = 40;
    float angle = 0, scaleX = 1, scaleY = 1, alpha = 1;
    float anchorX = .5f, anchorY = .5f;
    uint32_t afpBlend = 0;
    int32_t slot = -1; // stable movie identity, not its animation frame
    std::string package = "gmobject";
};
struct LongGraphicOverlay {
    int32_t category = 0, x = 0, y = 0;
    float scale = 1, alpha = 1;
};
struct LongGraphicDraws {
    std::vector<LongGraphicDraw> draws;
    // Original 101576F0 effect queue. Caller must present the original effect.
    std::vector<LongGraphicOverlay> overlays;
};
struct LongNoteGraphicStyle {
    std::string body, tail, background, tailEffect;
    int32_t tailLayer = 40;
};
LongNoteGraphicStyle selectLongNoteGraphic(int32_t color, int32_t top, int32_t side,
                                           uint8_t presentationFlags = 0); // 1015C1A0
struct LongNoteGraphicState {
    bool active = false;
    bool bodyVisible = true; // original reset hides it; holdEntry explicitly enables it
    MotionPoint head, tail;
    float angle = 0, bodyScaleX = 1, bodyScaleY = 1;
    float tailScale = 1, alpha = 1, effectScale = 1;
    bool headEffect = false, tailEffect = false;
};
// 1015C2D0: actual head/tail geometry. Retains original float stores.
void updateLongNoteGraphic(LongNoteGraphicState &, MotionPoint head, MotionPoint tail,
                           float stageScale);
// 1015C400: coincident endpoints and velocity orientation; body scale is retained.
void updateLongNoteGraphicDirection(LongNoteGraphicState &, MotionPoint, MotionVector);
LongGraphicDraws drawLongNoteGraphic(const LongNoteGraphicStyle &,
                                     const LongNoteGraphicState &); // 1015BF90

struct SwitchGraphicSegment {
    int32_t duration = 0;
    MotionPoint end;
    bool collection = false, special = false, secondarySpecial = false;
};
struct SwitchLongGraphicState {
    bool active = false, held = false;
    int32_t color = 0;
    MotionPoint head, trail;
    MotionVector velocity;
    int32_t trailTicks = 0, secondaryTrailTicks = 0, clipY = 1098; // original global 12477E34
    float alpha = 1, scale = 1, angle = 0;
    std::vector<SwitchGraphicSegment> segments; // original cap: 16
};
// 10157250: duration-based segment endpoints, including original integer truncation.
void updateSwitchLongGraphic(SwitchLongGraphicState &);
LongGraphicDraws drawSwitchLongGraphic(const SwitchLongGraphicState &); // 10156D40
} // namespace rb
