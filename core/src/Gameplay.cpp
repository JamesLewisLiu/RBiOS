#include "rb/Gameplay.hpp"
#include "rb/Binary.hpp"
#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace rb {
namespace {
int32_t add(int32_t a, int32_t b) {
    uint32_t v = uint32_t(a) + uint32_t(b);
    int32_t n;
    std::memcpy(&n, &v, 4);
    return n;
}
int32_t sub(int32_t a, int32_t b) {
    uint32_t v = uint32_t(a) - uint32_t(b);
    int32_t n;
    std::memcpy(&n, &v, 4);
    return n;
}
void detach(RuntimeNote &n) { // 0x10158B40 / 0x10158AD0, logical links only
    if (n.previous)
        n.previous->next = nullptr;
    if (n.next) {
        n.next->previous = nullptr;
        n.next->touch.inputBlocked = true;
    }
    n.previous = nullptr;
    n.touch.inputBlocked = true;
    n.next = nullptr;
}
void initial(RuntimeNote &n, int32_t grade, bool style) { // 0x10159A50 / 10159A80
    if (grade != 4)
        n.successful = true;
    n.deviation = n.judgment.relativeTick;
    detach(n);
    if (style)
        n.judgment.style = grade;
}
void cursor(RuntimeNote &n) {
    uint8_t bits = uint8_t(n.judgment.replayCursor) + uint8_t(1);
    std::memcpy(&n.judgment.replayCursor, &bits, 1);
}
void finish(RuntimeNote &n) {
    n.judgment.ended = true;
    detach(n);
} // 0x10159A20
} // namespace
void applyBaseNoteEvent(RuntimeNote &n, const JudgmentResult &r) {
    auto &j = n.judgment;
    if ((n.flags & 8) && r.grade != 4) {
        n.touch.cooldown = 2;
        std::unordered_set<RuntimeNote *> visited{&n};
        for (auto p = n.next; p; p = p->next) {
            if (!visited.insert(p).second)
                throw Error("cyclic original note links");
            p->touch.cooldown = 2; // 0x10158330
        }
    }
    if (j.kind == 1) {
        switch (r.reason) {
        case 3:
            initial(n, r.grade, true);
            cursor(n);
            break;
        case 4:
            cursor(n);
            break;
        case 6:
            cursor(n);
            finish(n);
            break;
        case 7:
        case 8:
            if (r.grade == 3)
                cursor(n);
            break;
        default:
            finish(n);
            break;
        }
    } else if (j.kind == 4) {
        switch (r.reason) {
        case 4:
            cursor(n);
            break;
        case 5:
            if (j.segmentIndex < 0)
                initial(n, r.grade, true);
            cursor(n);
            j.segmentIndex = std::max(0, std::min(j.switchCount, add(j.segmentIndex, 1)));
            j.segmentElapsed = sub(j.segmentElapsed, j.segmentDuration);
            if (j.segmentIndex < j.switchCount) {
                if (size_t(j.segmentIndex) >= n.switchDurations.size())
                    throw Error("missing original switch duration");
                j.segmentDuration = n.switchDurations[size_t(j.segmentIndex)];
            } else {
                j.segmentDuration = 0;
                j.completed = true;
                finish(n);
            }
            j.switchDirection = !j.switchDirection;
            break;
        case 7:
        case 8:
            if (r.grade == 3)
                cursor(n);
            break;
        default:
            finish(n);
            break;
        }
    } else {
        if (r.grade != 4) {
            initial(n, r.grade, false);
            finish(n);
        }
        cursor(n);
        finish(n);
    }
}
SubmittedJudgment consumeJudgment(RuntimeNote &n, const JudgmentResult &r, BattleScore &b,
                                  PlayerScore &p, StageScoreState &s) {
    if (r.grade < 0 || r.grade > 4)
        throw Error("invalid submitted judgment");
    applyBaseNoteEvent(n, r);
    SubmittedJudgment result{r, r, false, false, 0};
    bool dirty = true;
    if (r.reason == 3 || r.reason == 4)
        dirty = false;
    else if (r.reason == 5)
        dirty = n.judgment.ended;
    else if (r.reason == 7 || r.reason == 8) {
        if (r.grade != 3)
            return result;
        dirty = false;
        result.normalized.reason = 4;
    }
    if (!s.scoringDisabled) {
        ScoreEvent e{unsigned(n.side), unsigned(r.grade), result.normalized.reason, n.judgment.kind,
                     n.deviation,      n.special,         n.specialPenalty};
        submitScore(b, p, e, s.updateRemoteCombo, s.stageModeSix);
        result.scored = true;
        // 0x1015F6E0: completion bonus is separate from per-event points.
        int32_t done = 0;
        for (auto count : p.counts)
            done = add(done, count);
        bool gaugeAlive = s.gaugeMode == 0 || s.gaugeMode > 4 || b.gauge[s.gaugeMode - 1] > 0;
        if (done >= p.totalEvents && gaugeAlive) {
            if (n.side == 1 && s.remoteFinal) {
                p.maxCombo = std::clamp(s.remoteFinal->maxCombo, 0, 9999);
                b.score[1] = std::clamp(s.remoteFinal->score, 0, 9999);
                b.displayedScore[1] = std::min(b.displayedScore[1], b.score[1]);
            } else
                b.applyEffect(7, unsigned(n.side), 0, p.maxCombo, s.stageModeSix);
            if (!s.stageModeSix)
                result.medal = p.medal(s.difficulty, s.clearOption);
        }
    }
    s.processed = add(s.processed, 1);
    result.counted = true;
    if (dirty)
        s.dirty = true;
    return result;
}
std::optional<SubmittedJudgment> evaluateNote(RuntimeNote &n, TouchState &touch,
                                              const InputRules &rules, BattleScore &b,
                                              PlayerScore &p, StageScoreState &stage, CPUState *cpu,
                                              OriginalRandom *random,
                                              const std::vector<uint8_t> *replay) {
    auto &j = n.judgment;
    auto &t = n.touch;
    static_cast<NoteTiming &>(t) = static_cast<const NoteTiming &>(j);
    t.touchId = j.contactId;
    t.noteMode = j.style;
    t.segmentReverse = j.switchDirection;
    t.side = n.side;
    JudgmentQueries q;
    q.missed = [&] {
        auto geometry = rules.geometry;
        if (!geometry)
            geometry = [&](int32_t direction, int32_t margin) {
                return t.inField(rules.field, direction, margin, rules.touchScale);
            };
        return t.missed(rules.touchScale, rules.remoteWide, geometry);
    };
    q.pressed = [&] {
        bool found = t.firstPress(touch, rules.touchScale);
        j.contactId = t.touchId;
        return found;
    };
    q.holdCovered = [&] { return t.maintainHold(touch, rules.touchScale, rules.supplemental); };
    q.switchHeld = [&] { return t.maintainSwitch(touch, rules.touchScale, rules.supplemental); };
    q.switchPress = [&]() -> std::optional<int32_t> {
        int32_t reason = -1;
        bool found = t.switchPress(touch, rules.touchScale, reason);
        j.contactId = t.touchId;
        return found ? std::optional<int32_t>(reason) : std::nullopt;
    };
    if (cpu) {
        if (!random)
            throw Error("CPU requires original shared RNG");
        q.cpu = [&]() -> std::optional<JudgmentResult> {
            cpu->meter = p.justReflec; // original cpu+4 points to this PlayerScore
            auto r = cpu->evaluate({n.chartIndex, j.kind, n.flags, j.relativeTick, j.segmentIndex,
                                    j.segmentDuration, j.segmentElapsed},
                                   *random);
            if (r)
                return JudgmentResult{r->judgment, r->event};
            return {};
        };
    }
    q.replay = replay;
    auto r = dispatchJudgment(j, rules.mode, rules.timingMode, rules.rejectOuter, q);
    t.touchId = j.contactId;
    if (!r)
        return {};
    if (n.side == 0 && rules.degradeMiddle && r->grade == 1)
        r->grade = 2; // 0x10160620
    return consumeJudgment(n, *r, b, p, stage);
}
} // namespace rb
