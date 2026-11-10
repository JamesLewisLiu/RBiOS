#include "rb/StageSound.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace rb {
const std::array<StageSoundCue, 7> &stageSoundResources() {
    static const std::array<StageSoundCue, 7> resources{{
        {10192, "sys_shot_default1_just", "data/sound/sys_sd_rb6/sys_shot_default1_just.sd9"},
        {11312, "sys_shot_default1_great", "data/sound/sys_sd_rb6/sys_shot_default1_great.sd9"},
        {12432, "sys_shot_default1_good", "data/sound/sys_sd_rb6/sys_shot_default1_good.sd9"},
        {14752, "sys_shot_default1_long_end", "data/sound/sys_sd_rb6/sys_shot_default1_long_end.sd9"},
        {14832, "sys_shot_default1_long_on", "data/sound/sys_sd_rb6/sys_shot_default1_long_on.sd9"},
        {14912, "sys_shot_default1_bigbang", "data/sound/sys_sd_rb6/sys_shot_default1_bigbang.sd9"},
        {15072, "sys_shot_damage", "data/sound/sys_sd_rb6/sys_shot_damage.sd9"},
    }};
    return resources;
}
namespace {
std::optional<StageSoundCue> cueFor(int32_t id) {
    for (const auto &cue : stageSoundResources())
        if (cue.id == id)
            return cue;
    return std::nullopt;
}
} // namespace
std::optional<StageSoundCue> selectStageSound(int32_t side, int32_t kind, int32_t grade,
                                             int32_t reason, int32_t switchDirection,
                                             bool missSound) {
    if (side != 0 || grade < 0 || grade > 4)
        return std::nullopt;
    switch (reason) {
    case 0: case 1: case 2: case 3: case 5: case 6: case 11: case 12: break;
    default: return std::nullopt;
    }
    if (reason == 6 && grade != 4)
        grade = 0;
    if (kind == 3 && grade != 4)
        return cueFor(14912);
    if (grade != 4) {
        if (switchDirection == 0)
            return cueFor(14832);
        if (switchDirection == 1)
            return cueFor(14752);
    }
    const int32_t bank[] = {10192, 11312, 12432, 10192, missSound ? 15072 : 9472};
    return cueFor(bank[grade]);
}
void StageSoundQueue::reset() {
    tick_.reset();
    elapsed_.fill(0);
    pending_.fill(9472);
}
void StageSoundQueue::beginTick(int32_t tick) {
    if (tick_ && *tick_ == tick)
        return;
    if (tick_ && tick < *tick_)
        throw std::logic_error("stage sound ticks must be monotonic; reset before replay");
    if (std::any_of(pending_.begin(), pending_.end(), [](int32_t id) { return id != 9472; }))
        throw std::logic_error("stage sound requests must be drained before the next tick");
    const int64_t delta = tick_ ? int64_t(tick) - *tick_ : 1;
    for (auto &elapsed : elapsed_)
        elapsed = int32_t(std::min(int64_t(elapsed) + delta,
                                  int64_t(std::numeric_limits<int32_t>::max())));
    tick_ = tick;
}
void StageSoundQueue::submit(int32_t tick, int32_t side, int32_t kind, int32_t grade,
                             int32_t reason, int32_t switchDirection, bool missSound) {
    beginTick(tick);
    const auto cue = selectStageSound(side, kind, grade, reason, switchDirection, missSound);
    if (!cue)
        return;
    const auto id = cue->id;
    const size_t lane = id == 14912 ? 1 : id == 14832 ? 2 : id == 14752 ? 3 : 0;
    if (lane) {
        if (elapsed_[lane] > 2)
            pending_[lane] = id;
    } else if (pending_[0] != 15072 && (id == 15072 || elapsed_[0] > 2) &&
               (pending_[0] == 9472 || id == 15072 ||
                ((pending_[0] == 14912 || pending_[0] == 14992) && id < pending_[0]))) {
        pending_[0] = id;
    }
}
std::vector<StageSoundCue> StageSoundQueue::drain(int32_t tick) {
    beginTick(tick);
    std::vector<StageSoundCue> result;
    for (size_t i = 0; i < pending_.size(); ++i) {
        if (const auto cue = cueFor(pending_[i])) {
            result.push_back(*cue);
            elapsed_[i] = 0;
        }
        pending_[i] = 9472;
    }
    return result;
}
} // namespace rb
