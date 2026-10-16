#include "rb/NoteFrame.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace rb {
namespace {
int32_t wrap(uint32_t x) {
    int32_t y;
    std::memcpy(&y, &x, 4);
    return y;
}
int32_t add(int32_t a, int32_t b) {
    return wrap(uint32_t(a) + uint32_t(b));
}
int32_t sub(int32_t a, int32_t b) {
    return wrap(uint32_t(a) - uint32_t(b));
}
void release(RuntimeNote &n, NoteFrame &f) {
    n.judgment.completed = true;
    f.pendingPhase = NotePhase::release;
    f.pendingState = 0;
    f.transitionPending = true;
}
int32_t truncate(double value) {
    if (!std::isfinite(value) || double(value) < -2147483648. || double(value) >= 2147483648.)
        throw Error("invalid original exit interpolation");
    return int32_t(value);
}
} // namespace
void initializeNote(RuntimeNote &n, NoteFrame &f, const NoteInitialization &i,
                    const std::function<RuntimeNote *(int16_t)> &lookup) {
    auto &j = n.judgment;
    f.initializedAt = i.tick;
    f.previousId = i.previousId;
    f.lateTicks = sub(i.tick, i.chartStartTick);
    if (f.lateTicks <= 0 || i.pathMode == 1 || !i.pathDelay)
        j.relativeTick = sub(i.chartTravelTicks, f.lateTicks);
    else {
        j.relativeTick = i.chartTravelTicks;
        f.delayedTicks = f.lateTicks;
    }
    j.relativeTick = std::max(j.relativeTick, int32_t(10));
    if (n.side == 1)
        f.remoteSide = true;
    if ((n.flags & 8) && i.previousId != -1) {
        if (!lookup)
            throw Error("missing original linked-note lookup");
        n.previous = lookup(i.previousId);
        if (n.previous)
            n.previous->next = &n;
    }
    if (j.kind == 1 || j.kind == 4)
        j.durationTicks = i.durationTicks;
    n.specialPenalty = n.side != 1 || !i.remoteSession;
    f.allowSpecial = n.side != 0 || !i.remoteSession;
    static_cast<NoteTiming &>(n.touch) = static_cast<const NoteTiming &>(j);
}
void advanceLongState(RuntimeNote &n, NoteFrame &f) {
    auto &j = n.judgment;
    if (!f.stateFrame)
        n.touch.phase = add(j.relativeTick, j.durationTicks);
    n.touch.phase = sub(n.touch.phase, 1);
    j.segmentElapsed = add(j.segmentElapsed, 1);
    f.trailTicks = add(f.trailTicks, 1);
    f.secondaryTrailTicks = add(f.secondaryTrailTicks, 1);
    if (sub(j.switchCount, 1) <= j.segmentIndex) {
        const int32_t remaining =
            j.segmentIndex < 0 ? j.relativeTick : sub(j.segmentDuration, j.segmentElapsed);
        if (remaining < -9)
            release(n, f);
    }
    static_cast<NoteTiming &>(n.touch) = static_cast<const NoteTiming &>(j);
}
bool advanceExitState(RuntimeNote &n, NoteFrame &f) {
    auto &j = n.judgment;
    if (!j.durationTicks)
        throw Error("zero original exit duration");
    if (!f.stateFrame)
        n.touch.phase = add(j.relativeTick, j.durationTicks);
    n.touch.phase = sub(n.touch.phase, 1);
    // x87 division is stored to f32 before either multiplication.
    float fraction = float(double(n.touch.phase) / double(j.durationTicks));
    f.endpointX = add(f.x, truncate(double(f.exitX) * fraction));
    f.endpointY = add(f.y, truncate(double(f.exitY) * fraction));
    if (n.touch.phase <= 0)
        release(n, f);
    bool effect = false;
    switch (f.state) {
    case 0:
        f.pendingState = 1;
        f.transitionPending = true;
        [[fallthrough]];
    case 1:
        if ((j.kind == 1 || j.kind == 4) && j.held) {
            f.pendingState = 2;
            f.transitionPending = true;
        }
        break;
    case 2:
        f.pendingState = 3;
        f.transitionPending = true;
        [[fallthrough]];
    case 3:
        effect = f.stateTimer % 3 == 0;
        if ((j.kind != 1 && j.kind != 4) || !j.held) {
            f.pendingState = 0;
            f.transitionPending = true;
        }
        break;
    default:
        break;
    }
    return effect;
}
void fadeReleased(RuntimeNote &n, NoteFrame &f) {
    const float before = f.opacity;
    const float after = before - 0.15000001f;
    const float capped = std::min(before, after);
    f.opacity = std::max(0.f, capped);
    if (f.opacity <= 0)
        n.judgment.touchReleased = true;
}
bool finishNoteFrame(RuntimeNote &n, NoteFrame &f) {
    if (!f.active)
        return false;
    n.judgment.relativeTick = sub(n.judgment.relativeTick, 1);
    f.elapsed = add(f.elapsed, 1);
    n.touch.inputBlocked = false;
    n.touch.cooldown = std::max(int32_t(0), sub(n.touch.cooldown, 1));
    static_cast<NoteTiming &>(n.touch) = static_cast<const NoteTiming &>(n.judgment);
    return n.judgment.expired();
}
void selectInitialState(RuntimeNote &n, NoteFrame &f) {
    f.phase = (n.flags & 0x10)       ? NotePhase::topEntry
              : n.judgment.kind == 1 ? NotePhase::holdRoute
              : n.judgment.kind == 4 ? NotePhase::switchEntry
                                     : NotePhase::normal;
    f.pendingPhase = NotePhase::none;
    f.previousPhase = NotePhase::none;
    f.state = f.pendingState = f.stateFrame = f.stateTimer = 0;
    f.transitionPending = false;
}
void advanceNoteState(RuntimeNote &n, NoteFrame &f, MotionPosition &p, MotionTimeline &m,
                      NoteFrameContext &c) {
    if (!f.active)
        return;
    if (f.pendingPhase != NotePhase::none) {
        f.previousPhase = f.phase;
        f.phase = f.pendingPhase;
        f.pendingPhase = NotePhase::none;
        f.stateFrame = 0;
    }
    if (f.transitionPending) {
        f.state = f.pendingState;
        f.transitionPending = false;
        f.stateTimer = 0;
    }
    auto transition = [&](NotePhase next) {
        f.pendingPhase = next;
        f.pendingState = 0;
        f.transitionPending = true;
    };
    auto integrate = [&] {
        p.integrate(m);
        f.x = p.rounded.x;
        f.y = p.rounded.y;
        n.touch.x = f.x;
        n.touch.y = f.y;
    };
    auto delayed = [&] {
        for (int32_t i = 0; i < f.delayedTicks; ++i) {
            integrate();
            m.advance();
            n.judgment.relativeTick = sub(n.judgment.relativeTick, 1);
            f.elapsed = add(f.elapsed, 1);
        }
        f.delayedTicks = 0;
    };
    auto opacityUp = [&] {
        float before = f.opacity;
        float after = before + .050000001f;
        f.opacity = std::max(before, std::min(1.f, after));
    };
    auto reflected = [&] {
        return c.path.reflectionCount > 0 && c.path.reflectionCount <= 3 &&
               m.tick >=
                   truncate(double(m.reflectionTimes[size_t(c.path.reflectionCount - 1)]) + .5);
    };
    auto dynamicPath = [&] {
        bool force = false;
        if (f.pathReady)
            return force;
        const int32_t age = sub(c.tick, f.initializedAt);
        if (!f.copiedPath && n.side == 1 && c.path.type == 1 &&
            (!(n.flags & 8) || f.previousId == -1)) {
            if (age == 3) {
                if (!c.acceptRemoteRoute)
                    throw Error("missing original remote route handler");
                bool accepted = c.acceptRemoteRoute();
                if (n.touch.region == 1)
                    force = !accepted;
            }
        } else {
            int32_t remaining = n.judgment.segmentIndex < 0
                                    ? n.judgment.relativeTick
                                    : sub(n.judgment.segmentDuration, n.judgment.segmentElapsed);
            bool special = c.specialRouteAvailable ? c.specialRouteAvailable()
                                                   : c.path.type >= 7 && c.path.type <= 9 &&
                                                         remaining >= -9 && remaining <= 9;
            if (special) {
                if (!c.buildSpecialRoute)
                    throw Error("missing original special route builder");
                c.buildSpecialRoute();
            } else if (f.pathPacket) {
                if (!c.applyPathPacket)
                    throw Error("missing original route packet builder");
                c.applyPathPacket();
            } else if (age == 6 && n.touch.region == 1)
                force = true;
        }
        return force;
    };
    switch (f.phase) {
    case NotePhase::release:
        fadeReleased(n, f);
        break;
    case NotePhase::longTail:
        advanceLongState(n, f);
        break;
    case NotePhase::exit:
        if (advanceExitState(n, f) && c.holdPresentation)
            c.holdPresentation();
        break;
    case NotePhase::switchEntry:
    case NotePhase::holdRoute: {
        bool force = false;
        if (!f.stateFrame) {
            force =
                c.path.reflectionCount == 0 && f.pathReady && c.path.type != 1 && c.path.type != 2;
            delayed();
        }
        force = dynamicPath() || force;
        force = c.path.direction == 2 || force;
        integrate();
        opacityUp();
        if (f.phase == NotePhase::holdRoute) {
            f.endpointX = f.x;
            f.endpointY = f.y;
        }
        if (force || reflected() || f.forceTravel)
            transition(f.phase == NotePhase::holdRoute ? NotePhase::holdEntry
                                                       : NotePhase::switchTravel);
        break;
    }
    case NotePhase::normal:
        if (!f.stateFrame) {
            delayed();
            integrate();
            m.advance();
        }
        dynamicPath();
        integrate();
        opacityUp();
        if ((n.flags & 8) && m.tick == truncate(m.specialReflectionTime)) {
            bool event = m.tick == truncate(double(m.specialReflectionTime) + .5);
            for (int32_t k = 0; k < c.path.reflectionCount && k < 3; ++k)
                event = event || m.tick == truncate(double(m.reflectionTimes[size_t(k)]) + .5);
            if (event) {
                if (n.previous)
                    n.previous->next = nullptr;
                if (n.next) {
                    n.next->previous = nullptr;
                    n.next->touch.inputBlocked = true;
                }
                n.previous = n.next = nullptr;
                n.touch.inputBlocked = true;
            }
        }
        if (n.touch.region == 1 && n.judgment.relativeTick < -9)
            transition(NotePhase::missFade);
        break;
    case NotePhase::switchTravel:
        f.pathReady = true;
        if (!f.stateFrame) {
            f.trailX = f.x;
            f.trailY = f.y;
            f.switchTrail = true;
            if (f.delayedTicks > 0)
                delayed();
        }
        f.trailTicks = add(f.trailTicks, 1);
        integrate();
        opacityUp();
        if (!(n.side ? f.y > c.field.y(5) : f.y < c.field.y(4)))
            transition(NotePhase::longTail);
        break;
    case NotePhase::holdEntry:
        f.pathReady = true;
        if (!f.stateFrame) {
            f.endpointX = f.x;
            f.endpointY = f.y;
            f.holdTrail = true;
            if (m.speed <= 0)
                throw Error("invalid original hold entry speed");
            f.entryRemaining = std::min(n.judgment.durationTicks, truncate(380.0 / m.speed));
            if (f.delayedTicks > 0) {
                f.entryRemaining = sub(f.entryRemaining, f.delayedTicks);
                delayed();
            }
        }
        {
            float t = float(f.stateFrame);
            f.endpointX = sub(f.x, truncate(double(p.delta.x) * t));
            f.endpointY = sub(f.y, truncate(double(p.delta.y) * t));
            f.exitX = float(sub(f.endpointX, f.x));
            f.exitY = float(-truncate(double(p.delta.y) * t));
        }
        integrate();
        opacityUp();
        f.entryRemaining = sub(f.entryRemaining, 1);
        if (f.entryRemaining <= 0)
            transition(NotePhase::holdTravel);
        break;
    case NotePhase::holdTravel:
        if (!f.stateFrame) {
            f.exitX = float(sub(f.endpointX, f.x));
            f.exitY = float(sub(f.endpointY, f.y));
        }
        integrate();
        f.endpointX = add(f.x, truncate(f.exitX));
        f.endpointY = add(f.y, truncate(f.exitY));
        if (n.judgment.relativeTick <= 0)
            transition(NotePhase::exit);
        break;
    case NotePhase::missFade:
        integrate();
        if (n.judgment.kind == 1) {
            f.endpointX = add(f.x, truncate(f.exitX));
            f.endpointY = add(f.y, truncate(f.exitY));
        }
        {
            const float before = f.opacity, after = before - .15000001f;
            f.opacity = std::max(0.f, std::min(before, after));
            if (f.opacity <= 0)
                n.touch.missFlag = true;
        }
        break;
    case NotePhase::topEntry: {
        if (!f.stateFrame)
            f.tempoMarker = c.tempoMarker;
        if (!std::isfinite(c.bpm) || c.bpm <= 0)
            throw Error("invalid original top-entry tempo");
        const float start = float(c.chartStartTick);
        const float end = float(3600.0 / c.bpm + start);
        const float length = end - start;
        const float elapsed = float((double(c.tick) - start) / length);
        const float fraction = std::min(1.f, elapsed);
        const float size = float(.300000011920929 * fraction);
        f.scale = float(double(size) + .699999988079071);
        f.opacity = fraction;
        if (fraction == 1) {
            transition(n.judgment.kind == 1   ? NotePhase::holdEntry
                       : n.judgment.kind == 4 ? NotePhase::switchTravel
                                              : NotePhase::normal);
            if (!c.initializeTopTimeline)
                throw Error("missing original top timeline initializer");
            c.initializeTopTimeline();
            f.scale = 1;
            f.scalePercent = 100;
            f.pathReady = true;
            f.opacity = 1;
        }
        break;
    }
    default:
        throw Error("unrestored original note state");
    }
    f.stateFrame = add(f.stateFrame, 1);
    f.stateTimer = add(f.stateTimer, 1);
}
} // namespace rb
