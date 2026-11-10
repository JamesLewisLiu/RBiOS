#include "rb/Assets.hpp"
#include "rb/StageOrder.hpp"
#include "rb/StageScoreInitialization.hpp"
#include "rb/StageSession.hpp"
#include "rb/StageSound.hpp"
#include <filesystem>
#include <iostream>
#include <numeric>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--sort") {
            size_t n;
            while (std::cin >> n) {
                std::vector<StageOrderEntry> entries(n);
                for (size_t i = 0; i < n; ++i) {
                    entries[i].identity = uint32_t(i);
                    std::cin >> entries[i].active >> entries[i].relativeTick;
                }
                sortStageOrder(entries);
                for (auto &e : entries)
                    std::cout << e.identity << ' ';
                std::cout << '\n';
            }
            return 0;
        }
        if (argc < 2) {
            Chart chart;
            chart.version = 20;
            chart.bpm = 120;
            chart.durationMs = 20000;
            for (unsigned side = 0; side < 2; ++side) {
                ChartObject n;
                n.id = int16_t(side);
                n.group = -1;
                n.startMs = 0;
                n.travelMs = 1000;
                n.flags = 0x10;
                n.attributes[1] = uint8_t(side);
                n.parameters[1] = 0;
                chart.objects.push_back(n);
            }
            SheetOptions so;
            so.unequalSidesAllowed = true;
            auto sheet = MusicSheet::load(chart, so);
            sheet.prepare();
            SessionOptions options;
            options.inputModes = {1, 3};
            options.cpuDifficulty = 16;
            options.initialBattle =
                initializeStageScore(StageScoreInitialization::offline(sheet, 1));
            StageSession a(sheet, options), b(sheet, options);
            a.advanceToTick(1000, {});
            for (int t = -150; t <= 1000; ++t) {
                b.advanceToTick(t, {});
                if (t == 30)
                    b.cancelTouches();
            }
            auto x = a.snapshot(), y = b.snapshot();
            if (!x.complete || x.tick != 1000 || x.created != 2 || x.processed != 2 ||
                !x.notes.empty())
                throw Error("synthetic original stage lifecycle failed");
            for (unsigned side = 0; side < 2; ++side) {
                if (x.players[side].counts[0] != 1 || x.players[side].maxCombo != 1 ||
                    x.battle.score[side] != 56)
                    throw Error("synthetic original score failed");
                if (x.players[side].counts != y.players[side].counts ||
                    x.battle.score[side] != y.battle.score[side])
                    throw Error("stage step size changed deterministic score");
            }
            StageSoundQueue sounds;
            std::vector<int32_t> played;
            unsigned observedTicks = 0;
            options.onJudgment = [&](const SessionJudgmentEvent &event) {
                sounds.submit(event.tick, event.side, event.kind, event.result.original.grade,
                              event.result.original.reason, event.switchDirection);
            };
            options.onPresentationFrame = [&](const SessionSnapshot &snapshot) {
                ++observedTicks;
                for (const auto &cue : sounds.drain(snapshot.tick)) played.push_back(cue.id);
            };
            StageSession audible(sheet, options);
            audible.advanceToTick(1000, {});
            if (observedTicks != 1151 || played != std::vector<int32_t>{10192})
                throw Error("batched stage callbacks lost local sound or synthesized CPU sound");
            bool rejected = false;
            try {
                b.advanceToTick(999, {});
            } catch (const Error &) {
                rejected = true;
            }
            if (!rejected)
                throw Error("backwards clock accepted");
            std::cout << "StageSession: lifecycle, deterministic score, cancellation and clock "
                         "checks passed; score="
                      << x.battle.score[0] << ',' << x.battle.score[1] << '\n';
            return 0;
        }
        auto root = std::filesystem::path(argv[1]);
        auto catalog = readCatalog(readFile((root / "modules/reflecbeat.dll").string()));
        int limit = argc > 2 ? std::stoi(argv[2]) : 3, tested = 0;
        for (const auto &song : catalog)
            for (unsigned difficulty = 0; difficulty < 4; ++difficulty) {
                auto path = root / song.chartPath(difficulty);
                if (argc > 4 && path.filename().string() != argv[4])
                    continue;
                if (!std::filesystem::exists(path))
                    continue;
                for (int mode : {1, 0}) {
                    SheetOptions so;
                    so.localChartSide = argc > 3 ? std::stoi(argv[3]) : 0;
                    so.difficulty = int(difficulty);
                    so.unequalSidesAllowed = true;
                    auto sheet = MusicSheet::load(Chart::parse(readFile(path.string())), so);
                    sheet.prepare();
                    SessionOptions options;
                    options.initialBattle = initializeStageScore(
                        StageScoreInitialization::offline(sheet, song.levels[difficulty]));
                    options.inputModes = {mode, 3};
                    std::vector<int> received(sheet.objects.size());
                    options.onJudgment = [&](const SessionJudgmentEvent &e) {
                        if (e.result.scored)
                            ++received[size_t(e.object)];
                    };
                    int expected = int(sheet.objects.size()),
                        maxTick = millisecondsToTicks(sheet.chart.durationMs) + 661;
                    auto counts = sheet.counts.total;
                    auto originalObjects = sheet.objects;
                    try {
                        StageSession stage(std::move(sheet), options);
                        stage.advanceToTick(maxTick, {});
                        auto out = stage.snapshot();
                        std::cout << path.filename().string() << " mode=" << mode
                                  << " tick=" << out.tick << " complete=" << out.complete
                                  << " created=" << out.created << '/' << expected
                                  << " processed=" << out.processed
                                  << " active=" << out.notes.size() << " expected=" << counts[0]
                                  << ',' << counts[1] << " counts=";
                        for (auto &p : out.players)
                            std::cout << std::accumulate(p.counts.begin(), p.counts.end(), 0)
                                      << ',';
                        std::cout << '\n';
                        if (argc > 4)
                            for (size_t i = 0; i < received.size(); ++i) {
                                auto original = Chart::parse(readFile(path.string()));
                                const auto &n = original.objects[i];
                                int expectedEvents = 1 + original.additionalTicks(i);
                                if (n.attributes[3] == 4)
                                    expectedEvents += int(original.switchDurations(i).size());
                                if (received[i] != expectedEvents)
                                    std::cout << "OBJECT " << i << " kind=" << int(n.attributes[3])
                                              << " side=" << int(n.attributes[1])
                                              << " start=" << n.startMs << " travel=" << n.travelMs
                                              << " duration=" << n.parameters[0]
                                              << " flags=" << n.flags << " received=" << received[i]
                                              << " expected=" << expectedEvents << '\n';
                            }
                        if (mode == 1 && std::accumulate(out.players[0].counts.begin(),
                                                         out.players[0].counts.end(),
                                                         0) != counts[unsigned(so.localChartSide)])
                            throw Error("automatic local event count mismatch");
                        const int cpuCount = std::accumulate(out.players[1].counts.begin(),
                                                             out.players[1].counts.end(), 0);
                        int shortMissedHeads = 0;
                        for (size_t i = 0; i < originalObjects.size(); ++i) {
                            const auto &n = originalObjects[i];
                            if (n.side == 1 && n.attributes[3] == 1 && n.duration > 0 &&
                                n.duration <= 11 && received[i] == 1)
                                ++shortMissedHeads;
                        }
                        if (cpuCount != counts[unsigned(1 - so.localChartSide)] - shortMissedHeads)
                            throw Error("unexplained CPU event count mismatch");
                        if (!out.complete || out.created != expected || !out.notes.empty())
                            throw Error("stage did not drain original chart");
                    } catch (const std::exception &e) {
                        throw Error(path.filename().string() + " mode=" + std::to_string(mode) +
                                    ": " + e.what());
                    }
                }
                if (++tested >= limit) {
                    std::cout << "StageSession audited " << tested << " charts x2 modes\n";
                    return 0;
                }
            }
        return tested ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
