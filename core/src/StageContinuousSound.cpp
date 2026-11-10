#include "rb/StageContinuousSound.hpp"
namespace rb {
void StageContinuousSound::observe(int32_t side, int32_t kind, bool held, bool enabled) {
    if (enabled && side == 0 && (kind == 1 || kind == 4) && held) desired_ = true;
}
LoopChange StageContinuousSound::endFrame() {
    const auto change = desired_ == active_ ? LoopChange::none :
                        desired_ ? LoopChange::start : LoopChange::stop;
    active_ = desired_;
    return change;
}
const StageSoundCue &StageContinuousSound::resource() {
    static const StageSoundCue cue{14672, "sys_shot_default1_long_loop",
        "data/sound/sys_sd_rb6/sys_shot_default1_long_loop.sd9"};
    return cue;
}
} // namespace rb
