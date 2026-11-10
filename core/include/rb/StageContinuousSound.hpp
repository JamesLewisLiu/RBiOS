#pragma once
#include "StageSound.hpp"
namespace rb {
enum class LoopChange { none, start, stop };
// Default offline bank, original 10160620 aggregation and 10163CE0 lifecycle.
// Observe each evaluated actor before release; final ended actors can still hold.
class StageContinuousSound {
  public:
    void beginFrame() { desired_ = false; }
    void observe(int32_t side, int32_t kind, bool held, bool enabled = true);
    LoopChange endFrame();
    bool active() const { return active_; }
    void reset() { desired_ = active_ = false; }
    static const StageSoundCue &resource();
  private:
    bool desired_ = false, active_ = false;
};
} // namespace rb
