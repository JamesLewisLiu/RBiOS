#include "rb/JudgeDispatch.hpp"
#include "rb/Binary.hpp"
#include <cstring>

namespace rb {
namespace {
using Result = std::optional<JudgmentResult>;
Result event(int32_t grade, int32_t reason) {
    return JudgmentResult{grade, reason};
}
bool query(const std::function<bool()> &f) {
    if (!f)
        throw Error("required original judgment query is unavailable");
    return f();
}
int32_t subtract(int32_t a, int32_t b) {
    uint32_t bits = uint32_t(a) - uint32_t(b);
    int32_t n;
    std::memcpy(&n, &bits, 4);
    return n;
}
int32_t remaining(const DispatchNote &n) {
    return n.segmentIndex < 0 ? n.relativeTick : subtract(n.segmentDuration, n.segmentElapsed);
}
Result missedOrTick(DispatchNote &n, const JudgmentQueries &q) { // 0x1016AE40
    if (query(q.missed))
        return event(4, 2);
    if (n.isHoldScoringTick())
        return event(4, 4);
    return {};
}
Result completedHold(DispatchNote &n) { // 0x1016AD60
    if (n.isHoldScoringTick())
        return event(3, 4);
    if (n.completed)
        return event(3, 6);
    return {};
}
Result automaticSwitch(DispatchNote &n) { // 0x1016AD00
    if (n.kind == 4 && remaining(n) <= 0)
        return event(0, 5);
    if (n.isHoldScoringTick())
        return event(3, 4);
    return {};
}
Result automaticInitial(DispatchNote &n) { // 0x1016AE80
    if (n.kind == 4 || (n.kind == 1 && n.style != 6) || n.relativeTick > 0) {
        if (n.isHoldScoringTick())
            return event(3, 4);
        if (n.atSwitchBoundary())
            return event(0, 5);
        return {};
    }
    return event(0, n.kind == 1 ? 3 : 1);
}
Result forcedMiss(DispatchNote &n, const JudgmentQueries &q) { // 0x1016AE00
    if (!query(q.missed))
        return {};
    return event(4, n.kind == 4 ? 5 : 13);
}
Result manualHold(DispatchNote &n, const JudgmentQueries &q) { // 0x1016ADA0
    if (query(q.holdCovered))
        return completedHold(n);
    return event(4 - int(n.isHoldScoringTick()), 8);
}
Result manualSwitch(DispatchNote &n, uint32_t table, bool reject,
                    const JudgmentQueries &q) { // 0x1016AF00
    int32_t tick = remaining(n);
    if (tick < -9)
        return event(4, 5);
    if (n.segmentIndex == subtract(n.switchCount, 1) && !n.switchDirection && n.atSwitchBoundary())
        return event(0, 5);
    if (tick >= -9 && tick <= 9) {
        if (!q.switchPress)
            throw Error("original switch press query is unavailable");
        auto press = q.switchPress();
        if (press) {
            if (*press == 7)
                return {};
            int32_t grade = int32_t(classifyTiming(tick, table, true, reject));
            return grade == 6 ? Result{} : event(grade, 5);
        }
    }
    auto normal = timingWindows(table, false), alternate = timingWindows(table, true);
    if ((normal[6] > tick || tick > alternate[1]) && !query(q.switchHeld))
        return event(4 - int(n.isHoldScoringTick()), 8);
    if (n.isHoldScoringTick())
        return event(3, 4);
    return {};
}
Result manualInitial(DispatchNote &n, uint32_t table, bool reject,
                     const JudgmentQueries &q) { // 0x1016B050
    if (query(q.missed))
        return event(4, 2);
    if (n.kind == 4) {
        if (!q.switchPress)
            throw Error("original switch press query is unavailable");
        auto press = q.switchPress();
        if (press) {
            if (*press == 7)
                return event(4 - int(n.isHoldScoringTick()), 7);
            int32_t grade =
                int32_t(classifyTiming(remaining(n), table, n.segmentIndex > 0, reject));
            if (grade != 6)
                return event(grade, *press);
            n.contactId = -1;
            return {};
        }
    } else if (query(q.pressed)) {
        bool ignoreOuter = reject && !(n.kind == 1 && n.style != 6);
        int32_t grade = int32_t(classifyTiming(n.relativeTick, table, false, ignoreOuter));
        return grade == 6 ? Result{} : event(grade, 0);
    }
    if (n.isHoldScoringTick())
        return event(4, 4);
    return {};
}
int replayNext(const DispatchNote &n, const JudgmentQueries &q) {
    int32_t i = int32_t(n.replayCursor) + 1;
    if (i < 0)
        throw Error("invalid original replay cursor");
    return size_t(i) < q.replay->size() ? (*q.replay)[size_t(i)] : -1;
}
int replayLast(const JudgmentQueries &q) {
    return q.replay->empty() ? -1 : q.replay->back();
}
Result replayInitial(DispatchNote &n, const JudgmentQueries &q) { // 0x1016B740
    if (query(q.missed))
        return event(4, 2);
    if (!q.replay)
        return missedOrTick(n, q);
    if (!n.isHoldScoringTick()) {
        if (n.kind == 1) {
            if (n.relativeTick > 0 || n.replayCursor != -1)
                return {};
        } else if ((n.kind == 4 ? remaining(n) : n.relativeTick) > 0)
            return {};
    }
    int code = replayNext(n, q);
    switch (code) {
    case 0:
        return event(0, 12);
    case 1:
        return event(0, 11);
    case 2:
        return event(1, 11);
    case 3:
        return event(2, 11);
    case 4:
        return event(3, 4);
    case 5:
        return event(4, 4);
    default:
        return {};
    }
}
Result replayHold(DispatchNote &n, const JudgmentQueries &q) { // 0x1016B520, replay present
    if (!n.isHoldScoringTick()) {
        if (!n.completed)
            return {};
        return event(replayLast(q) == 4 ? 3 : 4, 6);
    }
    return event(replayNext(n, q) == 4 ? 3 : 4, 4);
}
Result replaySwitch(DispatchNote &n, const JudgmentQueries &q) { // 0x1016B290, replay present
    if (n.kind != 4 || remaining(n) > 0) {
        if (!n.isHoldScoringTick())
            return {};
        return event(replayNext(n, q) == 4 ? 3 : 4, 4);
    }
    switch (replayNext(n, q)) {
    case 0:
    case 1:
        return event(0, 5);
    case 2:
        return event(1, 5);
    case 3:
        return event(2, 5);
    default:
        return event(4, 5);
    }
}
Result cpuInitial(DispatchNote &n, const JudgmentQueries &q) { // 0x1016B1F0
    if (query(q.missed))
        return event(4, 2);
    if (!q.cpu)
        return missedOrTick(n, q);
    auto r = q.cpu();
    if (r || !n.isHoldScoringTick())
        return r;
    return event(4, 4);
}
Result cpuSwitch(DispatchNote &n, const JudgmentQueries &q) { // 0x1016B190
    if (q.cpu) {
        auto r = q.cpu();
        if (r) {
            r->reason = 5;
            return r;
        }
    }
    if (n.isHoldScoringTick())
        return event(3, 4);
    return {};
}
} // namespace
std::optional<JudgmentResult> dispatchJudgment(DispatchNote &n, int32_t mode, uint32_t table,
                                               bool reject, const JudgmentQueries &q) {
    if ((n.kind == 1 || n.kind == 4) && n.held && !(n.kind == 4 && n.completed)) {
        Result r;
        if (n.kind == 1) {
            switch (mode) {
            case 0:
                r = manualHold(n, q);
                break;
            case 1:
            case 5:
                r = completedHold(n);
                break;
            case 3:
                r = q.replay ? replayHold(n, q) : completedHold(n);
                break;
            case 4:
                if (n.completed)
                    r = event(3, 6);
                break;
            default:
                r = event(4, 8);
                break;
            }
        } else {
            switch (mode) {
            case 0:
                r = manualSwitch(n, table, reject, q);
                break;
            case 1:
            case 5:
                r = automaticSwitch(n);
                break;
            case 3:
                r = q.replay ? replaySwitch(n, q) : cpuSwitch(n, q);
                break;
            case 4:
                r = forcedMiss(n, q);
                break;
            default:
                r = event(4, 8);
                break;
            }
        }
        if (r && (r->grade == 4 || r->reason == 8)) {
            n.held = false;
            n.contactId = -1;
        }
        return r;
    }
    if ((n.kind == 1 || n.kind == 4) && n.completed) {
        int32_t reason = n.kind == 1 ? 6 : 5;
        if (mode == 3 && q.replay) {
            int code = replayLast(q);
            if (n.kind == 1) {
                if (code == 4)
                    return event(3, 6);
            } else {
                if (code == 0 || code == 1)
                    return event(0, 5);
                if (code == 2)
                    return event(1, 5);
                if (code == 3)
                    return event(2, 5);
            }
        }
        return n.touchReleased ? event(4, reason) : Result{};
    }
    Result r;
    switch (mode) {
    case 0:
        r = manualInitial(n, table, reject, q);
        break;
    case 1:
    case 5:
        r = automaticInitial(n);
        break;
    case 2:
        r = missedOrTick(n, q);
        break;
    case 3:
        r = q.replay ? replayInitial(n, q) : cpuInitial(n, q);
        break;
    case 4:
        r = forcedMiss(n, q);
        break;
    default:
        return missedOrTick(n, q); // original default skips postprocessing
    }
    if (!r)
        return {};
    if (n.kind == 1) {
        if (n.style == 6) {
            r->reason = 3;
            n.held = r->grade != 4;
        } else if (r->grade != 4) {
            r->reason = 7;
            n.held = true;
            r->grade = 4 - int(n.isHoldScoringTick());
        } else {
            r->reason = 4;
            n.held = false;
        }
    } else if (n.kind == 4) {
        if (r->reason != 4 && r->reason != 7)
            r->reason = 5;
        n.held = r->grade != 4 || r->reason == 7;
    }
    return r;
}
} // namespace rb
