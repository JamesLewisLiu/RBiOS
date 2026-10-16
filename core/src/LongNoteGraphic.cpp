#include "rb/LongNoteGraphic.hpp"
#include <cmath>

namespace rb {
namespace {
float directionAngle(float x, float y) {
    const float quarter = static_cast<float>(std::atan2(1.0, 0.0));
    const float direction = static_cast<float>(std::atan2(double(y), double(x)));
    return quarter - direction;
}
float bodyLength(MotionPoint a, MotionPoint b) {
    const float dx = static_cast<float>(std::abs(int64_t(a.x) - b.x));
    const float dy = static_cast<float>(std::abs(int64_t(a.y) - b.y));
    const float square = static_cast<float>(double(dx) * dx + double(dy) * dy);
    const float length = static_cast<float>(std::sqrt(double(square)));
    return static_cast<float>(double(length) / 312.0);
}
LongGraphicDraw draw(std::string name, bool movie, MotionPoint p, int layer, float angle, float sx,
                     float sy, float alpha, float ay = .5f, uint32_t blend = 0, int slot = -1,
                     std::string label = {}) {
    const auto package = name == "exp_loop" ? "gmexp_effect" : "gmobject";
    return {std::move(name),
            std::move(label),
            movie,
            p.x,
            p.y,
            layer,
            angle,
            sx,
            sy,
            alpha,
            .5f,
            ay,
            blend,
            slot,
            package};
}
void validate(const SwitchLongGraphicState &s) {
    if (s.segments.size() > 16 || s.color < 0 || s.color > 1)
        throw Error("switch graphic outside original segment/color bounds");
}
} // namespace
LongNoteGraphicStyle selectLongNoteGraphic(int32_t color, int32_t top, int32_t side,
                                           uint8_t flags) {
    if (color < 0 || color > 1 || side < 0 || side > 1)
        throw Error("long graphic outside original color/side table");
    const auto suffix = top == 1 && !(flags & 8) ? "top" : color ? "blue" : "red";
    return {std::string("long_body_") + suffix, std::string("long_tail_") + suffix,
            std::string("long_tail_bg_") + suffix,
            std::string("eff_long_tail_") + (color ? "blue" : "red"), top == 1 ? 39 : 40};
}
void updateLongNoteGraphic(LongNoteGraphicState &s, MotionPoint head, MotionPoint tail,
                           float stageScale) {
    s.head = head;
    s.tail = tail;
    s.angle = directionAngle(static_cast<float>(int64_t(tail.x) - head.x),
                             static_cast<float>(int64_t(head.y) - tail.y));
    s.bodyScaleX = stageScale;
    s.bodyScaleY = bodyLength(head, tail);
    s.effectScale = stageScale;
}
void updateLongNoteGraphicDirection(LongNoteGraphicState &s, MotionPoint p, MotionVector v) {
    s.head = s.tail = p;
    s.angle = directionAngle(-v.x, v.y);
}
LongGraphicDraws drawLongNoteGraphic(const LongNoteGraphicStyle &style,
                                     const LongNoteGraphicState &s) {
    LongGraphicDraws out;
    if (!s.active)
        return out;
    if (s.headEffect)
        out.draws.push_back(draw("exp_loop", true, s.head, 61, 0, 1, 1, s.alpha, .5f, 0, 0));
    if (s.tailEffect)
        out.draws.push_back(draw(style.tailEffect, true, s.tail, 37, s.angle, s.effectScale,
                                 s.effectScale, s.alpha, .5f, 0, 1));
    if (s.bodyVisible)
        out.draws.push_back(draw(style.body, false, s.tail, 62, s.angle, s.bodyScaleX, s.bodyScaleY,
                                 s.alpha, 0, 8));
    out.draws.push_back(draw(style.tail, false, s.tail, style.tailLayer, s.angle, s.tailScale,
                             s.tailScale, s.alpha));
    out.draws.push_back(draw(style.background, false, s.tail, 61, s.angle, s.tailScale, s.tailScale,
                             s.alpha, .5f, 8));
    return out;
}
void updateSwitchLongGraphic(SwitchLongGraphicState &s) {
    validate(s);
    int64_t elapsed = 0;
    for (auto &part : s.segments) {
        elapsed += part.duration;
        part.end = s.head;
        if (s.trailTicks > 0 && s.secondaryTrailTicks < elapsed) {
            const auto delta = elapsed - s.secondaryTrailTicks;
            part.end.x = static_cast<int32_t>(double(s.head.x) - double(delta) * s.velocity.x);
            part.end.y = static_cast<int32_t>(double(s.head.y) - double(delta) * s.velocity.y);
        }
    }
    s.angle = directionAngle(-s.velocity.x, s.velocity.y);
}
LongGraphicDraws drawSwitchLongGraphic(const SwitchLongGraphicState &s) {
    validate(s);
    LongGraphicDraws out;
    if (!s.active || s.trailTicks <= 0)
        return out;
    MotionPoint previous = s.head;
    bool visible = true;
    for (size_t i = 0; i < s.segments.size(); ++i) {
        const auto &part = s.segments[i];
        if (s.trail.y >= previous.y)
            visible = false;
        if (s.clipY > part.end.y) {
            MotionPoint begin = s.clipY >= previous.y ? previous : MotionPoint{s.head.x, s.clipY};
            MotionPoint end = s.trail.y >= part.end.y ? s.trail : part.end;
            const bool finalTail = i + 1 == s.segments.size() && !(i % 2);
            if (visible) {
                out.draws.push_back(draw(i % 2 ? "swich_long_body_off" : "swich_long_body_on", true,
                                         begin, 62, s.angle, s.scale, bodyLength(begin, end),
                                         s.alpha, 1, 0, int(i), s.held ? "on" : "off"));
                if (finalTail) {
                    out.draws.push_back(draw("long_tail_switch", false, end, 40, s.angle, s.scale,
                                             s.scale, s.alpha));
                    out.draws.push_back(draw("long_tail_bg_switch", true, end, 40, s.angle, s.scale,
                                             s.scale, s.alpha, .5f, 0, 48, s.held ? "on" : "off"));
                } else {
                    out.draws.push_back(draw(i % 2 ? "swich_on" : "swich_off", true, end, 40,
                                             s.angle, s.scale, s.scale, s.alpha, .5f, 0,
                                             16 + int(i), "off"));
                }
                if (part.collection)
                    out.draws.push_back(draw("eff_justcollection", true, end, 38, 0, s.scale,
                                             s.scale, 1, .5f, 0, 32 + int(i)));
            }
            if (part.special && !finalTail)
                out.overlays.push_back({part.secondarySpecial ? (s.color != 0) + 1 : 0, end.x,
                                        end.y, s.scale, s.alpha});
        }
        previous = part.end;
    }
    return out;
}
} // namespace rb
