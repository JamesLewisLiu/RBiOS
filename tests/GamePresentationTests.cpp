#include "rb/GamePresentation.hpp"
#include "rb/StageScoreInitialization.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
using namespace rb;
int main(int argc, char **argv) {
    try {
        auto q = originalSpriteQuad(92, 312, 384, 900, 0, 1, .5f, .5f, 0);
        if (q[0].x != 338 || q[0].y != 1056 || q[2].y != 900)
            throw Error("original body alignment");
        q = originalSpriteQuad(72, 72, 100, 200, 0, 2, 1, .5f, .5f);
        if (q[0].x != 28 || q[0].y != 236 || q[3].x != 172 || q[3].y != 164)
            throw Error("original nonuniform quad");
        GamePresentation invalid("no-resources-required", 0);
        invalid.noteCreated(1, {});
        invalid.noteReleased(1);
        bool rejected = false;
        try {
            invalid.noteReleased(1);
        } catch (const Error &) {
            rejected = true;
        }
        if (!rejected)
            throw Error("presenter accepted duplicate release");
        if (argc > 1) {
            const std::filesystem::path root = argv[1];
            auto songs = readCatalog(readFile((root / "modules/reflecbeat.dll").string()));
            unsigned limit = argc > 2 ? unsigned(std::stoul(argv[2])) : 3, charts = 0;
            const std::string filter = argc > 3 ? argv[3] : "";
            const unsigned stride = argc > 4 ? unsigned(std::stoul(argv[4])) : 30;
            if (!limit || !stride)
                throw Error("presentation audit limit/stride must be positive");
            uint64_t frames = 0, draws = 0;
            std::array<uint64_t, 5> kinds{};
            std::set<std::string> resources, unresolved;
            const auto report = [&] {
                std::cout << "Presentation: " << charts << " songs, " << frames
                          << " sampled frames, " << draws << " draws, " << resources.size()
                          << " original bitmaps\n";
                std::cout << "Object kinds:";
                for (auto n : kinds)
                    std::cout << ' ' << n;
                std::cout << '\n';
                for (const auto &s : unresolved)
                    std::cout << "UNRESOLVED " << s << '\n';
            };
            for (const auto &song : songs)
                for (unsigned d = 0; d < 4; ++d) {
                    const auto path = root / song.chartPath(d);
                    if (!std::filesystem::exists(path))
                        continue;
                    if (path.filename().string().find(filter) == std::string::npos ||
                        song.levels[d] <= 0 || song.levels[d] >= 128)
                        continue;
                    SheetOptions so;
                    so.difficulty = int(d);
                    so.unequalSidesAllowed = true;
                    auto sheet = MusicSheet::load(Chart::parse(readFile(path.string())), so);
                    sheet.prepare();
                    for (const auto &o : sheet.objects)
                        if (o.attributes[3] >= 0 && o.attributes[3] < int(kinds.size()))
                            ++kinds[unsigned(o.attributes[3])];
                    GamePresentation presenter(root.string(), d);
                    SessionOptions options;
                    options.inputModes = {1, 3};
                    options.initialBattle = initializeStageScore(
                        StageScoreInitialization::offline(sheet, song.levels[d]));
                    options.onNoteCreated = [&](int32_t id, const auto &bindings) {
                        presenter.noteCreated(id, bindings);
                    };
                    options.onNoteReleased = [&](int32_t id) { presenter.noteReleased(id); };
                    options.onPresentationFrame = [&](const SessionSnapshot &s) {
                        presenter.advance(s);
                        if (s.tick == -150 || s.tick % int32_t(stride) == 0) {
                            auto frame = presenter.frame(s);
                            ++frames;
                            draws += frame.sprites.size();
                            for (const auto &draw : frame.sprites) {
                                const auto &b = presenter.bitmap(draw.package, draw.texture);
                                if (b.bgra.empty())
                                    throw Error("presenter bitmap empty");
                                resources.insert(draw.package + "/" + draw.texture);
                            }
                            unresolved.insert(frame.unresolved.begin(), frame.unresolved.end());
                        }
                    };
                    const auto end = millisecondsToTicks(sheet.chart.durationMs) + 661;
                    StageSession stage(std::move(sheet), options);
                    stage.advanceToTick(end, {});
                    if (!stage.snapshot().complete)
                        throw Error("presentation stage did not complete");
                    std::cout << path.filename().string() << " presentation passed\n";
                    if (++charts >= limit) {
                        report();
                        return 0;
                    }
                }
            if (!charts)
                throw Error("no original charts matched presentation audit");
            report();
            return 0;
        }
        std::cout << "PASS GamePresentation alignment, nonuniform scale and lifecycle checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
