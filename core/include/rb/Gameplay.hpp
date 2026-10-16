#pragma once
#include "CPU.hpp"
#include "JudgeDispatch.hpp"
#include "Score.hpp"
#include "Touch.hpp"

namespace rb {
// Restored base-note state, independent of AFP presentation objects. The caller
// supplies original chart preprocessing and time/motion updates; this is not a
// replacement stage initializer or a second timing algorithm.
struct RuntimeNote {
    DispatchNote judgment;
    NoteTouchState touch;
    int32_t chartIndex = -1, deviation = 0, side = 0;
    uint32_t flags = 0;
    bool successful = false, special = false, specialPenalty = false; // +377/379/426
    std::vector<int32_t> switchDurations;
    RuntimeNote *previous = nullptr, *next = nullptr; // original +248/+252
};
// Only base-note logic is restored here. AFP actors/sound/UI commands still need
// their original presenters; callers must not treat these functions as rendering.
void applyBaseNoteEvent(RuntimeNote &, const JudgmentResult &); // 0x10159E10
struct StageScoreState {
    bool scoringDisabled = false, dirty = false, updateRemoteCombo = true, stageModeSix = false;
    int32_t processed = 0;
    unsigned gaugeMode = 0, difficulty = 0;
    int32_t clearOption = 0;
    struct RemoteFinal {
        int32_t score, maxCombo;
    };
    std::optional<RemoteFinal> remoteFinal;
};
struct SubmittedJudgment {
    JudgmentResult original, normalized;
    bool counted = false, scored = false;
    int32_t medal = 0;
};
SubmittedJudgment consumeJudgment(RuntimeNote &, const JudgmentResult &, BattleScore &,
                                  PlayerScore &, StageScoreState &); // logic subset of 0x1015F8C0
struct InputRules {
    int32_t mode = 0;
    uint32_t timingMode = 0;
    bool rejectOuter = false, supplemental = false, remoteWide = false, degradeMiddle = false;
    float touchScale = 1; // original hit-box parameter, unrelated to display zoom
    FieldGeometry field;
    std::function<bool(int32_t, int32_t)> geometry; // optional derived-note override of +172
};
std::optional<SubmittedJudgment> evaluateNote(RuntimeNote &, TouchState &, const InputRules &,
                                              BattleScore &, PlayerScore &, StageScoreState &,
                                              CPUState *cpu = nullptr,
                                              OriginalRandom *random = nullptr,
                                              const std::vector<uint8_t> *replay = nullptr);
} // namespace rb
