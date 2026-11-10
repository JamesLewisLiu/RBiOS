#include "rb/MenuSelection.hpp"
#include <algorithm>
#include <stdexcept>
namespace rb {
namespace { void valid(int32_t difficulty) {
    if (difficulty < 0 || difficulty >= 4) throw std::invalid_argument("difficulty outside original four slots");
} }
DifficultyMenuResult DifficultyMenu::update(const DifficultyMenuInput &input) {
    valid(selected);
    if (!input.debounceEnabled) cooldown = 0;
    auto nextSelection = selected;
    bool confirmed = false;
    if (input.panel == -1) {
        if (cooldown <= 0 && input.next) {
            for (int i = 0; i < 4; ++i) {
                nextSelection = (nextSelection + 1) % 4;
                if (enabled[unsigned(nextSelection)]) break;
            }
            cooldown = 5;
        } else if (input.confirm) confirmed = true;
    } else if (input.panel == 0 || input.panel == 1) {
        if (enabled[unsigned(input.panel)]) nextSelection = input.panel;
    } else if (input.panel == 2 || input.panel == 3) {
        switch (selected) {
        case 0: case 1:
            if (enabled[2]) nextSelection = 2;
            else if (enabled[3]) nextSelection = 3;
            break;
        case 2: if (enabled[3]) nextSelection = 3; break;
        case 3: if (enabled[2]) nextSelection = 2; break;
        }
    }
    const bool changed = nextSelection != selected;
    selected = nextSelection;
    if (input.detail) showDetail = !showDetail;
    if (cooldown > 0) --cooldown;
    return {selected, changed, confirmed, input.detail};
}
StageSoundCue difficultyMenuSound(int32_t difficulty) {
    valid(difficulty);
    static const std::array<StageSoundCue, 4> sounds{{
        {16272, "sys_vox_basic", "data/sound/sys_sd_rb6/sys_vox_basic.sd9"},
        {16352, "sys_vox_medium", "data/sound/sys_sd_rb6/sys_vox_medium.sd9"},
        {16432, "sys_vox_hard", "data/sound/sys_sd_rb6/sys_vox_hard.sd9"},
        {16512, "sys_vox_whitehard", "data/sound/sys_sd_rb6/sys_vox_whitehard.sd9"},
    }};
    return sounds[unsigned(difficulty)];
}
bool sameDifficultyAudio(const std::array<uint8_t, 4> &suffix, int32_t a, int32_t b) {
    valid(a); valid(b);
    if (std::all_of(suffix.begin(), suffix.end(), [](uint8_t c) { return c == 32; })) return true;
    return suffix[unsigned(a)] == suffix[unsigned(b)];
}
std::array<bool, 4> difficultyMenuEnabled(int32_t initial,
    const std::array<uint8_t, 4> &suffix, const std::array<bool, 4> &eligibility) {
    valid(initial);
    std::array<bool, 4> result{};
    for (int32_t i = 0; i < 4; ++i)
        result[unsigned(i)] = (i <= initial || eligibility[unsigned(i)]) && sameDifficultyAudio(suffix, i, initial);
    return result;
}
} // namespace rb
