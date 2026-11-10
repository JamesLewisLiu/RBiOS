#include "rb/StageSound.hpp"
#include "rb/Assets.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--batch") {
            int side, kind, grade, reason, phase, miss;
            while (std::cin >> side >> kind >> grade >> reason >> phase >> miss) {
                auto cue = selectStageSound(side, kind, grade, reason, phase, miss != 0);
                std::cout << (cue ? cue->id : 9472) << '\n';
            }
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--resources") {
            uint64_t bytes = 0;
            for (const auto &cue : stageSoundResources()) {
                const auto wave = Wave::parse(readFile((std::filesystem::path(argv[2]) / cue.path).string()));
                uint64_t samples = 0;
                bool audible = false;
                for (size_t block = 0; block < wave.blockCount(); ++block) {
                    const auto pcm = wave.decodeBlock(block);
                    samples += pcm.size();
                    for (const auto sample : pcm) audible = audible || sample != 0;
                }
                if (wave.channels != 2 || wave.sampleRate != 44100 || !audible || samples != wave.frames * 2)
                    throw std::runtime_error("original effect PCM failed: " + std::string(cue.key));
                bytes += samples * 2;
            }
            if (bytes > 16 * 1024 * 1024) throw std::runtime_error("effects exceed preparation limit");
            std::cout << "PASS 7 original effects decoded to " << bytes << " PCM bytes in memory\n";
            return 0;
        }
        size_t checks = 0;
        auto expect = [&](bool value) {
            ++checks;
            if (!value) throw std::runtime_error("stage sound check " + std::to_string(checks));
        };
        expect(selectStageSound(0, 0, 0, 0)->id == 10192);
        expect(selectStageSound(0, 0, 1, 0)->id == 11312);
        expect(selectStageSound(0, 0, 2, 0)->id == 12432);
        expect(selectStageSound(0, 0, 3, 0)->id == 10192);
        expect(!selectStageSound(0, 0, 4, 0));
        expect(selectStageSound(0, 0, 4, 0, -1, true)->id == 15072);
        expect(!selectStageSound(1, 0, 0, 0));
        expect(!selectStageSound(0, 0, 6, 0));
        for (int reason : {-1, 4, 7, 8, 9, 10, 13})
            expect(!selectStageSound(0, 1, 3, reason));
        expect(selectStageSound(0, 1, 2, 6)->id == 10192);
        expect(selectStageSound(0, 3, 2, 0, 0)->id == 14912);
        expect(selectStageSound(0, 4, 2, 5, 0)->id == 14832);
        expect(selectStageSound(0, 4, 2, 5, 1)->id == 14752);
        StageSoundQueue queue;
        queue.drain(-2); queue.drain(-1);
        queue.submit(0, 0, 0, 1, 0);
        queue.submit(0, 0, 0, 0, 0); // First ordinary cue wins this frame.
        queue.submit(0, 0, 3, 0, 0);
        queue.submit(0, 0, 4, 0, 5, 0);
        queue.submit(0, 0, 4, 0, 5, 1);
        auto cues = queue.drain(0);
        expect(cues.size() == 4);
        expect(cues[0].id == 11312 && cues[1].id == 14912 && cues[2].id == 14832 && cues[3].id == 14752);
        expect(queue.drain(0).empty());
        for (int tick = 1; tick <= 2; ++tick) {
            queue.submit(tick, 0, 0, 0, 0);
            expect(queue.drain(tick).empty());
        }
        queue.submit(3, 0, 0, 0, 0);
        expect(queue.drain(3).size() == 1);
        queue.submit(4, 0, 0, 4, 0, -1, true); // Damage bypasses cooldown.
        expect(queue.drain(4).at(0).id == 15072);
        queue.submit(7, 0, 0, 0, 0);
        queue.submit(7, 0, 0, 4, 0, -1, true); // Damage supersedes ordinary hit.
        expect(queue.drain(7).at(0).id == 15072);
        bool rejected = false;
        try { queue.drain(6); } catch (const std::logic_error &) { rejected = true; }
        expect(rejected);
        queue.reset();
        expect(queue.drain(-150).empty());
        expect(stageSoundResources().size() == 7);
        std::cout << "PASS " << checks << " stage sound checks\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
