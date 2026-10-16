#pragma once
#include "Gameplay.hpp"

namespace rb {
enum class NotePhase : uint32_t {
    none = 0,
    release = 0x10159AF0,
    exit = 0x10159B30,
    longTail = 0x1015A1A0,
    switchTravel = 0x1015A470,
    holdEntry = 0x1015A5E0,
    holdTravel = 0x1015A540,
    missFade = 0x1015A740,
    switchEntry = 0x1015B0D0,
    holdRoute = 0x1015B250,
    normal = 0x1015B3F0,
    topEntry = 0x1015B590
};
// Typed state fields of the original CNoteBase. An unknown state is never
// implicitly advanced as one of the restored states below.
struct NoteFrame {
    bool active = false, remoteSide = false, allowSpecial = false;
    int32_t initializedAt = 0, elapsed = 0, lateTicks = 0, delayedTicks = 0;
    int32_t stateFrame = 0, stateTimer = 0, state = 0, pendingState = 0;
    NotePhase phase = NotePhase::none, pendingPhase = NotePhase::none;
    NotePhase previousPhase = NotePhase::none;
    bool transitionPending = false;
    int32_t x = 0, y = 0, endpointX = 0, endpointY = 0;
    float exitX = 0, exitY = 0, opacity = 1;
    int32_t trailTicks = 0, secondaryTrailTicks = 0;
    int32_t entryRemaining = 0;
    int32_t trailX = 0, trailY = 0;
    int16_t previousId = -1;
    bool pathReady = false, copiedPath = false, pathPacket = false, forceTravel = false;
    bool holdTrail = false, switchTrail = false;
    float scale = 1;
    float tempoMarker = 0;
    int32_t scalePercent = 100;
};
struct NoteInitialization {
    int32_t tick = 0, chartStartTick = 0, chartTravelTicks = 0;
    int32_t pathMode = 0, pathDelay = 0;
    int32_t durationTicks = 0; // kind 1 chart+344; kind 4 original 0x101493D0 result
    bool remoteSession = false;
    int16_t previousId = -1;
};
void initializeNote(RuntimeNote &, NoteFrame &, const NoteInitialization &,
                    const std::function<RuntimeNote *(int16_t)> &lookup = {}); // 10159090
void advanceLongState(RuntimeNote &, NoteFrame &);                             // 1015A1A0
// Returns whether original 101589B0 presentation callback was requested.
bool advanceExitState(RuntimeNote &, NoteFrame &); // 10159B30
void fadeReleased(RuntimeNote &, NoteFrame &);     // 10159AF0 + 10004660
// Logical suffix of 1015AC00. Call after state update; motion/presentation are
// separate. Returns original virtual+100 deletion predicate after decrement.
bool finishNoteFrame(RuntimeNote &, NoteFrame &);
void selectInitialState(RuntimeNote &, NoteFrame &); // 1015B720
struct NotePathState {
    int32_t type = 0, reflectionCount = 0, direction = 0;
};
struct NoteFrameContext {
    int32_t tick = 0;
    int32_t chartStartTick = 0;
    float bpm = 120;       // exact original 10148E80 result at chart end tick
    float tempoMarker = 0; // original global 1247932C
    FieldGeometry field;
    NotePathState path;
    // Required only for original dynamic route branches. Mutate timeline/context
    // as the original helper does. No network is performed by this module.
    std::function<bool()> acceptRemoteRoute;
    std::function<bool()> specialRouteAvailable;
    std::function<void()> buildSpecialRoute, applyPathPacket;
    std::function<void()> reflectionPresentation, holdPresentation;
    std::function<void()> initializeTopTimeline;
};
// Original scheduler 10008D60 then state function. Frame-end remains separate.
void advanceNoteState(RuntimeNote &, NoteFrame &, MotionPosition &, MotionTimeline &,
                      NoteFrameContext &);
} // namespace rb
