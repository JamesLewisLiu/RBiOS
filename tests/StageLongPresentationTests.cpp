#include "rb/StageScoreInitialization.hpp"
#include "rb/StageSession.hpp"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc == 1) {
            Chart chart;
            chart.version = 20;
            chart.bpm = 120;
            chart.durationMs = 10000;
            ChartObject object;
            object.id = 0;
            object.group = -1;
            object.travelMs = 1000;
            object.flags = 0x10;
            object.attributes[3] = 4;
            object.parameters[1] = 0;
            chart.objects.push_back(object);
            for (int i = 0; i < 16; ++i) {
                ChartExtra extra;
                extra.object = 0;
                extra.timeMs = 1000 + i * 250;
                extra.durationMs = 250;
                chart.extras.push_back(extra);
            }
            SheetOptions so;
            so.unequalSidesAllowed = true;
            auto sheet = MusicSheet::load(chart, so);
            sheet.prepare();
            SessionOptions options;
            options.inputModes = {1, 3};
            options.initialBattle =
                initializeStageScore(StageScoreInitialization::offline(sheet, 1));
            int created = 0, released = 0, previous = -151, frames = 0;
            bool lastBodySeen = false;
            options.onNoteCreated = [&](int, const auto &bindings) {
                if (bindings.size() != 51)
                    throw Error("16-segment allocation count");
                ++created;
            };
            options.onNoteReleased = [&](int) { ++released; };
            StageSession *session = nullptr;
            options.onPresentationFrame = [&](const SessionSnapshot &view) {
                if (view.tick != previous + 1)
                    throw Error("batched presentation lost tick");
                previous = view.tick;
                ++frames;
                for (const auto &note : view.notes)
                    for (const auto &draw : note.longGraphic.draws)
                        if (draw.movie && draw.slot == 15)
                            lastBodySeen = true;
                const auto again = session->snapshot();
                if (again.tick != view.tick || again.created != view.created ||
                    again.processed != view.processed || again.notes.size() != view.notes.size())
                    throw Error("snapshot advanced state");
            };
            StageSession stage(std::move(sheet), options);
            session = &stage;
            stage.advanceToTick(1261, {});
            if (created != 1 || released != 1 || !lastBodySeen || frames != 1151)
                throw Error("16-segment presentation lifecycle");
            std::cout << "PASS 16-segment fixture, 51 bindings, immutable snapshots, " << frames
                      << " batched 60Hz callbacks\n";
            return 0;
        }
        const std::filesystem::path root = argc > 1 ? argv[1] : "..";
        const int limit = argc > 2 ? std::stoi(argv[2]) : 100000;
        const int side = argc > 3 ? std::stoi(argv[3]) : 0;
        const int mode = argc > 4 ? std::stoi(argv[4]) : 1;
        const auto catalog = readCatalog(readFile((root / "modules/reflecbeat.dll").string()));
        uint64_t frames = 0, views = 0, draws = 0, bindings = 0, created = 0, released = 0;
        int tested = 0, maxSegments = 0, minX = INT32_MAX, maxX = INT32_MIN;
        int minY = INT32_MAX, maxY = INT32_MIN;
        std::map<std::string, uint64_t> resources, combinations;
        for (const auto &song : catalog) {
            for (unsigned difficulty = 0; difficulty < 4 && tested < limit; ++difficulty) {
                const auto path = root / song.chartPath(difficulty);
                if (!std::filesystem::exists(path))
                    continue;
                SheetOptions so;
                so.localChartSide = side;
                so.difficulty = int(difficulty);
                so.unequalSidesAllowed = true;
                auto sheet = MusicSheet::load(Chart::parse(readFile(path.string())), so);
                sheet.prepare();
                const auto objects = sheet.objects;
                std::map<int, const SheetObject *> byId;
                for (const auto &object : objects)
                    byId[object.source.id] = &object;
                SessionOptions options;
                options.initialBattle = initializeStageScore(
                    StageScoreInitialization::offline(sheet, song.levels[difficulty]));
                options.inputModes = {mode, 3};
                std::map<int, std::vector<SessionMovieBinding>> active;
                int previousTick = -151;
                options.onNoteCreated = [&](int id, const std::vector<SessionMovieBinding> &b) {
                    if (!active.emplace(id, b).second)
                        throw Error("reused live presentation ID");
                    std::set<int> slots;
                    for (const auto &binding : b) {
                        if (!slots.insert(binding.slot).second)
                            throw Error("duplicate original movie slot");
                        if (binding.poolIndex < 0 || binding.poolIndex >= 37)
                            throw Error("invalid original movie pool");
                    }
                    const auto &object = *byId.at(id);
                    if (object.side == 0 && object.attributes[3] == 4) {
                        const auto count = std::min(size_t(16), object.extraIndices.size());
                        maxSegments = std::max(maxSegments, int(count));
                        for (size_t i = 0; i < count; ++i)
                            for (int slot : {int(i), 16 + int(i), 32 + int(i)})
                                if (!slots.count(slot))
                                    throw Error("hidden switch movie was not allocated");
                        if (!slots.count(48))
                            throw Error("switch background was not allocated");
                    }
                    ++created;
                    bindings += b.size();
                };
                options.onNoteReleased = [&](int id) {
                    if (!active.erase(id))
                        throw Error("released unknown presentation ID");
                    ++released;
                };
                options.onPresentationFrame = [&](const SessionSnapshot &snapshot) {
                    if (snapshot.tick != previousTick + 1)
                        throw Error("presentation skipped a 60Hz tick");
                    previousTick = snapshot.tick;
                    ++frames;
                    for (const auto &note : snapshot.notes) {
                        const auto &expected = active.at(note.object);
                        if (note.movieBindings.size() != expected.size())
                            throw Error("snapshot changed allocation set");
                        if (note.kind != 1 && note.kind != 4)
                            continue;
                        ++views;
                        const auto &object = *byId.at(note.object);
                        const auto key = std::to_string(note.kind) + "/" +
                                         std::to_string(object.attributes[1]) + "/" +
                                         std::to_string(object.attributes[2]) + "/" +
                                         std::to_string(note.side);
                        ++combinations[key];
                        if (note.side && !note.longGraphic.draws.empty())
                            throw Error("invented opponent body component");
                        for (const auto &draw : note.longGraphic.draws) {
                            if (!std::isfinite(draw.angle) || !std::isfinite(draw.scaleX) ||
                                !std::isfinite(draw.scaleY) || !std::isfinite(draw.alpha))
                                throw Error("nonfinite original long draw geometry");
                            if (draw.movie) {
                                bool found = false;
                                for (const auto &binding : expected)
                                    found = found || (binding.slot == draw.slot &&
                                                      binding.resource == draw.resource &&
                                                      binding.package == draw.package);
                                if (!found)
                                    throw Error(
                                        "visible movie missing original allocation binding");
                            }
                            if (draw.resource.rfind("long_body_", 0) == 0 && draw.anchorY != 0)
                                throw Error("wrong original long body anchor");
                            if (draw.resource.rfind("swich_long_body_", 0) == 0 &&
                                draw.anchorY != 1)
                                throw Error("wrong original switch body anchor");
                            ++resources[draw.resource];
                            ++draws;
                            minX = std::min(minX, draw.x);
                            maxX = std::max(maxX, draw.x);
                            minY = std::min(minY, draw.y);
                            maxY = std::max(maxY, draw.y);
                        }
                    }
                };
                const int maxTick = millisecondsToTicks(sheet.chart.durationMs) + 661;
                StageSession stage(std::move(sheet), options);
                stage.advanceToTick(maxTick, {});
                const auto result = stage.snapshot();
                if (!result.complete || !active.empty() || !result.notes.empty())
                    throw Error("presentation lifecycle did not drain");
                ++tested;
                if (tested % 100 == 0)
                    std::cerr << "presentation charts=" << tested << " side=" << side
                              << " mode=" << mode << '\n';
            }
            if (tested >= limit)
                break;
        }
        std::cout << "{\"charts\":" << tested << ",\"side\":" << side << ",\"mode\":" << mode
                  << ",\"frames\":" << frames << ",\"long_views\":" << views
                  << ",\"draws\":" << draws << ",\"bindings\":" << bindings
                  << ",\"created\":" << created << ",\"released\":" << released
                  << ",\"max_segments\":" << maxSegments << ",\"draw_bounds\":[" << minX << ','
                  << maxX << ',' << minY << ',' << maxY << "],\"resources\":{";
        bool first = true;
        for (const auto &[key, count] : resources) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << '"' << key << "\":" << count;
        }
        std::cout << "},\"combinations_kind_color_top_side\":{";
        first = true;
        for (const auto &[key, count] : combinations) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << '"' << key << "\":" << count;
        }
        std::cout << "}}\n";
        return tested ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
