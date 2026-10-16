#include "rb/NoteFrame.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <sstream>
using namespace rb;
namespace {
std::array<uint8_t, 1220> wire{};
int32_t get(size_t offset) {
    int32_t v;
    std::memcpy(&v, wire.data() + offset, 4);
    return v;
}
float real(size_t offset) {
    float v;
    std::memcpy(&v, wire.data() + offset, 4);
    return v;
}
void put(size_t offset, int32_t value) {
    std::memcpy(wire.data() + offset, &value, 4);
}
void floating(size_t offset, float value) {
    std::memcpy(wire.data() + offset, &value, 4);
}
void run(char op, int32_t tick, bool remote) {
    RuntimeNote n;
    NoteFrame f;
    auto &j = n.judgment;
    j.kind = get(448 + 288);
    n.side = get(448 + 328);
    n.flags = uint32_t(get(448 + 300));
    j.relativeTick = get(192);
    j.durationTicks = get(296);
    j.segmentElapsed = get(360);
    j.segmentIndex = get(364);
    j.segmentDuration = get(368);
    j.switchCount = get(448 + 396);
    j.completed = wire[308];
    j.held = wire[309];
    j.touchReleased = wire[409];
    j.ended = wire[376];
    n.touch.phase = get(300);
    n.touch.inputBlocked = wire[411];
    n.touch.cooldown = get(420);
    n.specialPenalty = wire[426];
    f.x = get(100);
    f.y = get(104);
    f.phase = NotePhase(uint32_t(get(116)));
    f.pendingPhase = NotePhase(uint32_t(get(128)));
    f.previousPhase = NotePhase(uint32_t(get(120)));
    f.state = get(132);
    f.pendingState = get(136);
    f.transitionPending = wire[140];
    f.active = wire[20];
    f.stateFrame = get(144);
    f.stateTimer = get(148);
    f.elapsed = get(200);
    f.initializedAt = get(196);
    f.lateTicks = get(204);
    f.delayedTicks = get(208);
    f.endpointX = get(316);
    f.endpointY = get(320);
    f.exitX = real(332);
    f.exitY = real(336);
    f.trailTicks = get(344);
    f.secondaryTrailTicks = get(348);
    f.opacity = real(280);
    f.remoteSide = wire[410];
    f.allowSpecial = wire[427];
    f.pathReady = wire[378];
    f.copiedPath = wire[425];
    f.pathPacket = wire[381];
    f.forceTravel = wire[416];
    f.holdTrail = wire[328];
    f.switchTrail = wire[340];
    f.entryRemaining = get(324);
    f.trailX = get(352);
    f.trailY = get(356);
    n.touch.missFlag = wire[408];
    n.touch.region = get(448 + 284);
    f.scale = real(268);
    f.scalePercent = get(276);
    f.tempoMarker = real(288);
    bool effect = false;
    if (op == 'A') {
        MotionPosition p;
        p.position = {real(168), real(172)};
        p.delta = {real(176), real(180)};
        p.previousDelta = {real(184), real(188)};
        p.rounded = {f.x, f.y};
        p.suppressOnce = wire[424];
        MotionTimeline m;
        m.speed = real(864 + 312);
        m.velocity = {real(864 + 316), real(864 + 320)};
        m.tick = get(864 + 12);
        m.index = get(864);
        m.previousIndex = get(864 + 4);
        m.paused = wire[864 + 16];
        m.pauseTicks = get(864 + 20);
        for (int k = 0; k < get(864 + 8); ++k)
            m.segments.push_back({{real(864 + 216 + k * 16), real(864 + 220 + k * 16)},
                                  real(864 + 224 + k * 16),
                                  real(864 + 228 + k * 16)});
        for (size_t k = 0; k < 3; ++k)
            m.reflectionTimes[k] = real(864 + 324 + k * 4);
        NoteFrameContext c;
        c.tick = tick;
        c.chartStartTick = get(448 + 316);
        c.path = {get(864 + 44), get(864 + 32), get(864 + 64)};
        c.initializeTopTimeline = [] {};
        c.holdPresentation = [&] { effect = true; };
        advanceNoteState(n, f, p, m, c);
        put(100, f.x);
        put(104, f.y);
        floating(168, p.position.x);
        floating(172, p.position.y);
        floating(176, p.delta.x);
        floating(180, p.delta.y);
        floating(184, p.previousDelta.x);
        floating(188, p.previousDelta.y);
        wire[424] = p.suppressOnce;
    } else if (op == 'I') {
        NoteInitialization i;
        i.tick = tick;
        i.chartStartTick = get(448 + 316);
        i.chartTravelTicks = get(448 + 320);
        i.durationTicks = get(448 + 344);
        i.pathMode = get(864 + 40);
        i.pathDelay = get(864 + 44);
        i.remoteSession = remote;
        i.previousId = -1;
        initializeNote(n, f, i, [](int16_t) { return static_cast<RuntimeNote *>(nullptr); });
    } else if (op == 'L')
        advanceLongState(n, f);
    else if (op == 'E')
        effect = advanceExitState(n, f);
    else if (op == 'R')
        fadeReleased(n, f);
    else if (op == 'F')
        finishNoteFrame(n, f);
    else if (op == 'S')
        selectInitialState(n, f);
    else
        throw Error("unknown frame fixture");
    put(192, j.relativeTick);
    put(296, j.durationTicks);
    put(360, j.segmentElapsed);
    put(364, j.segmentIndex);
    put(368, j.segmentDuration);
    wire[308] = j.completed;
    wire[409] = j.touchReleased;
    put(300, n.touch.phase);
    wire[411] = n.touch.inputBlocked;
    put(420, n.touch.cooldown);
    wire[426] = n.specialPenalty;
    put(116, int32_t(f.phase));
    put(128, int32_t(f.pendingPhase));
    put(120, int32_t(f.previousPhase));
    put(132, f.state);
    put(136, f.pendingState);
    wire[140] = f.transitionPending;
    put(144, f.stateFrame);
    put(148, f.stateTimer);
    put(196, f.initializedAt);
    put(200, f.elapsed);
    put(204, f.lateTicks);
    put(208, f.delayedTicks);
    put(316, f.endpointX);
    put(320, f.endpointY);
    put(344, f.trailTicks);
    floating(332, f.exitX);
    floating(336, f.exitY);
    put(348, f.secondaryTrailTicks);
    floating(280, f.opacity);
    wire[410] = f.remoteSide;
    wire[427] = f.allowSpecial;
    wire[378] = f.pathReady;
    wire[328] = f.holdTrail;
    wire[340] = f.switchTrail;
    put(324, f.entryRemaining);
    put(352, f.trailX);
    put(356, f.trailY);
    wire[408] = n.touch.missFlag;
    floating(268, f.scale);
    put(276, f.scalePercent);
    floating(288, f.tempoMarker);
    if (op == 'S') {
        put(112, 0);
        put(120, 0);
        put(124, 0);
        put(152, 0);
        put(156, 0);
        wire[141] = 1;
    }
    static const char digits[] = "0123456789abcdef";
    for (size_t k = 0; k < 448; ++k)
        std::cout << digits[wire[k] >> 4] << digits[wire[k] & 15];
    std::cout << ' ' << effect << '\n';
}
} // namespace
int main(int argc, char **) {
    try {
        if (argc > 1) {
            char op;
            std::string hex;
            int tick, remote;
            while (std::cin >> op >> hex >> tick >> remote) {
                if (hex.size() != wire.size() * 2)
                    throw Error("frame fixture length");
                for (size_t k = 0; k < wire.size(); ++k)
                    wire[k] = uint8_t(std::stoul(hex.substr(2 * k, 2), nullptr, 16));
                run(op, tick, remote != 0);
            }
            return 0;
        }
        int count = 0;
        auto check = [&](bool v) {
            ++count;
            if (!v)
                throw Error("note frame assertion");
        };
        RuntimeNote n;
        NoteFrame f;
        NoteInitialization i;
        n.judgment.kind = 1;
        i.chartTravelTicks = 60;
        i.chartStartTick = -150;
        i.tick = -100;
        i.durationTicks = 90;
        initializeNote(n, f, i);
        check(n.judgment.relativeTick == 10);
        check(n.judgment.durationTicks == 90);
        f.active = true;
        selectInitialState(n, f);
        check(f.phase == NotePhase::holdRoute);
        f.stateFrame = 0;
        n.judgment.relativeTick = -81;
        advanceLongState(n, f);
        check(n.touch.phase == 8);
        check(n.judgment.completed);
        f.opacity = 1;
        for (int k = 0; k < 7; ++k)
            fadeReleased(n, f);
        check(n.judgment.touchReleased);
        n.judgment.relativeTick = 0;
        n.judgment.ended = false;
        n.touch.cooldown = 2;
        n.touch.inputBlocked = true;
        check(!finishNoteFrame(n, f));
        check(n.judgment.relativeTick == -1 && n.touch.cooldown == 1 && !n.touch.inputBlocked);
        f.active = false;
        check(!finishNoteFrame(n, f) && n.judgment.relativeTick == -1);
        std::cout << "PASS " << count << " note frame assertions\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
