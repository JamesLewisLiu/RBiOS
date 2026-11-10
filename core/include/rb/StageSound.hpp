#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rb {
struct StageSoundCue {
    int32_t id = 9472;
    const char *key = "";
    const char *path = "";
    // Original playback argument; ignored by the SD9 branch of 0x1006BC80.
    // Kept for provenance, never interpreted as playback gain.
    int32_t priority = 50;
};
// Original default profile bank. Paths refer to unchanged imported SD9 files.
const std::array<StageSoundCue, 7> &stageSoundResources();
// 0x1015E620 -> 0x10158910 -> selection portion of 0x10163A10.
// switchDirection is note +372 AFTER applying the note event, not chart index.
std::optional<StageSoundCue> selectStageSound(int32_t side, int32_t kind, int32_t grade,
                                             int32_t reason, int32_t switchDirection = -1,
                                             bool missSound = false);
// Recovered four pending lanes and >2-tick cooldown. Call drain once for EVERY
// simulation tick (60 Hz), including empty ticks, before advancing to the next.
// Presentation may accumulate drained cues across a catch-up interval.
class StageSoundQueue {
  public:
    void submit(int32_t tick, int32_t side, int32_t kind, int32_t grade, int32_t reason,
                int32_t switchDirection = -1, bool missSound = false);
    std::vector<StageSoundCue> drain(int32_t tick);
    void reset();
  private:
    void beginTick(int32_t tick);
    std::optional<int32_t> tick_;
    std::array<int32_t, 4> elapsed_{};
    std::array<int32_t, 4> pending_{{9472, 9472, 9472, 9472}};
};
} // namespace rb
