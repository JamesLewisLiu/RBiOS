#pragma once
#include "Gameplay.hpp"
#include "LongNoteGraphic.hpp"
#include "MusicSheet.hpp"
#include "NoteGraphic.hpp"
#include "TouchInput.hpp"
#include <memory>
namespace rb {
struct SessionSnapshot;
struct SessionMovieBinding {
    int32_t slot = -1, poolIndex = -1;
    std::string resource, package = "gmobject";
};
struct SessionJudgmentEvent {
    int32_t tick = 0, object = 0, side = 0, kind = 0, relativeTick = 0;
    SubmittedJudgment result;
    int32_t switchDirection = -1; // recovered note +372 after evaluation
};
struct SessionOptions {
    BattleScore initialBattle; // supply recovered original 101554C0 configuration
    uint32_t cpuDifficulty = 0, randomSeed = 77;
    std::array<int32_t, 2> inputModes{{0, 3}}; // local manual / opponent CPU
    InputRules input;
    std::function<void(const SessionJudgmentEvent &)> onJudgment;     // in-memory observer only
    std::function<void(const SessionSnapshot &)> onPresentationFrame; // exact 60 Hz, after step
    std::function<void(int32_t, const std::vector<SessionMovieBinding> &)> onNoteCreated;
    std::function<void(int32_t)> onNoteReleased;
    int32_t endTick = 1000; // original minimum completion tick, global 1247930C (10149010)
};
struct SessionNoteView {
    int32_t object = 0, side = 0, kind = 0, x = 0, y = 0, relativeTick = 0;
    float scale = 1, opacity = 1, direction = 0;
    int32_t createdTick = 0, lifetimeTicks = 0, movieLabelTick = 0;
    bool held = false, special = false;
    NoteGraphicDraws graphic;
    LongGraphicDraws longGraphic;
    // Full original allocation set, including currently hidden AFP instances.
    // Head slots are -1/-2; long presenter slots match LongGraphicDraw.slot.
    std::vector<SessionMovieBinding> movieBindings;
};
struct SessionSnapshot {
    int32_t tick = -150, created = 0, processed = 0;
    bool complete = false;
    std::array<PlayerScore, 2> players;
    BattleScore battle;
    std::vector<SessionNoteView> notes;
    std::vector<std::string> unsupportedPresentation;
    std::string failure;
};
class StageSession {
  public:
    StageSession(MusicSheet, SessionOptions);
    ~StageSession();
    StageSession(StageSession &&) noexcept;
    StageSession &operator=(StageSession &&) noexcept;
    void advanceToTick(int32_t, const std::vector<TouchPoint> &);
    SessionSnapshot snapshot() const;
    void cancelTouches();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace rb
