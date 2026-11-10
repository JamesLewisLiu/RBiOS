#include "rb/MenuSelection.hpp"
#include "rb/Assets.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--batch") {
            int selected, mask, panel, cooldown, next, confirm, detail, debounce;
            while (std::cin >> selected >> mask >> panel >> cooldown >> next >> confirm >> detail >> debounce) {
                DifficultyMenu menu; menu.selected = selected; menu.cooldown = cooldown;
                for (int i = 0; i < 4; ++i) menu.enabled[i] = (mask & (1 << i)) != 0;
                auto result = menu.update({panel, bool(next), bool(confirm), bool(detail), bool(debounce)});
                std::cout << result.difficulty << ' ' << menu.cooldown << ' ' << result.confirmed << ' ' << menu.showDetail << '\n';
            }
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--resources") {
            uint64_t bytes = 0;
            for (int32_t difficulty = 0; difficulty < 4; ++difficulty) {
                const auto cue = difficultyMenuSound(difficulty);
                const auto wave = Wave::parse(readFile((std::filesystem::path(argv[2]) / cue.path).string()));
                uint64_t samples = 0; bool audible = false;
                for (size_t block = 0; block < wave.blockCount(); ++block) {
                    const auto pcm = wave.decodeBlock(block); samples += pcm.size();
                    for (auto value : pcm) audible = audible || value != 0;
                }
                if (wave.channels != 2 || samples != wave.frames * 2 || !audible)
                    throw Error("original menu voice PCM mismatch");
                bytes += samples * 2;
            }
            if (bytes > 16 * 1024 * 1024) throw Error("menu voices exceed preparation limit");
            std::cout << "PASS four original menu voices decoded in memory: " << bytes << " bytes\n";
            return 0;
        }
        auto require = [](bool ok) { if (!ok) throw std::runtime_error("difficulty menu regression"); };
        for (int i = 0; i < 4; ++i) {
            const auto sound = difficultyMenuSound(i);
            require(sound.id == 16272 + 80 * i && sound.priority == 50);
            require(std::string(sound.path).find("data/sound/sys_sd_rb6/sys_vox_") == 0);
        }
        DifficultyMenu menu;
        require(menu.update({2}).difficulty == 2);
        require(menu.update({2}).difficulty == 3);
        require(menu.update({3}).difficulty == 2);
        menu.enabled = {{true, false, false, true}};
        require(menu.update({1}).difficulty == 2);
        require(menu.update({0}).difficulty == 0);
        require(menu.update({3}).difficulty == 3);
        require(menu.update({-1, true}).difficulty == 0 && menu.cooldown == 4);
        for (int i = 0; i < 4; ++i) require(!menu.update({-1, true}).changed);
        require(menu.update({-1, true}).difficulty == 3);
        require(menu.update({-1, false, true}).confirmed);
        require(!menu.update({0, false, true}).confirmed);
        require(menu.update({-1, false, false, true}).detailChanged && menu.showDetail);
        const std::array<uint8_t, 4> suffix{{32, 32, 'h', 'h'}};
        require(sameDifficultyAudio(suffix, 0, 1) && !sameDifficultyAudio(suffix, 0, 2));
        require(difficultyMenuEnabled(2, suffix, {{false,false,false,true}}) == std::array<bool,4>{{false,false,true,true}});
        bool rejected = false; try { menu.selected = 4; menu.update({}); } catch (const std::invalid_argument &) { rejected = true; }
        require(rejected);
        std::cout << "PASS difficulty menu selection, masks, debounce, confirm and audio-group gates\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
