#include "rb/Assets.hpp"
#include "rb/Chart.hpp"
#include "rb/Judgment.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--property-roundtrip") {
#ifdef _WIN32
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            Bytes input;
            char value;
            while (std::cin.get(value)) {
                if (input.size() >= 16 * 1024 * 1024)
                    throw Error("property input exceeds limit");
                input.push_back(uint8_t(value));
            }
            const auto output = PropertyDocument::decode(input).encode();
            std::cout.write(reinterpret_cast<const char *>(output.data()), output.size());
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--rules") {
            char type;
            while (std::cin >> type) {
                if (type == 'T') {
                    int32_t tick;
                    uint32_t mode;
                    bool alternate, rejectOuter;
                    if (!(std::cin >> tick >> mode >> alternate >> rejectOuter))
                        throw Error("invalid timing input");
                    std::cout << int(classifyTiming(tick, mode, alternate, rejectOuter)) << '\n';
                } else if (type == 'S') {
                    Chart chart;
                    ChartObject note;
                    note.attributes[3] = 4;
                    size_t count;
                    if (!(std::cin >> note.startMs >> note.travelMs >> count) || count > 1000)
                        throw Error("invalid switch input");
                    chart.objects.push_back(note);
                    for (size_t i = 0; i < count; ++i) {
                        ChartExtra extra;
                        if (!(std::cin >> extra.timeMs >> extra.durationMs))
                            throw Error("invalid switch segment");
                        chart.extras.push_back(extra);
                    }
                    std::cout << chart.additionalTicks(0) << '\n';
                } else if (type == 'H') {
                    NoteTiming note;
                    if (!(std::cin >> note.kind >> note.relativeTick >> note.durationTicks >>
                          note.segmentElapsed >> note.segmentIndex >> note.segmentDuration >>
                          note.ended))
                        throw Error("invalid note timing input");
                    std::cout << note.isHoldScoringTick() << ' ' << note.atSwitchBoundary() << ' '
                              << note.expired() << '\n';
                } else {
                    throw Error("unknown rule input");
                }
            }
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--texture-raw") {
            IfsArchive a(readFile(argv[2]));
            auto textures = textureList(a);
            if (textures.empty())
                throw Error("no textures");
            auto b = texturePixels(a, textures.front());
#ifdef _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            std::cout.write(reinterpret_cast<const char *>(b.data()), b.size());
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--seed") {
            auto v = randomSeed(uint32_t(std::stoull(argv[2])));
            for (auto x : v)
                std::cout << x << ' ';
            std::cout << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--long-ms") {
            Chart c;
            ChartObject o;
            o.attributes[3] = 1;
            o.parameters[0] = int16_t(std::stoi(argv[2]));
            c.objects.push_back(o);
            std::cout << c.additionalTicks(0) << '\n';
            return 0;
        }
        if (argc == 3 && std::string(argv[1]) == "--audio-raw") {
            auto w = Wave::parse(readFile(argv[2]));
#ifdef _WIN32
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            for (size_t i = 0; i < w.blockCount(); ++i) {
                auto b = w.decodeBlock(i);
                std::cout.write(reinterpret_cast<const char *>(b.data()), b.size() * 2);
            }
            return 0;
        }
        if (argc != 2)
            throw Error("usage: rbinspect GAME_ROOT | --seed N");
        std::filesystem::path root = argv[1];
        size_t charts = 0, objects = 0, ifs = 0, audio = 0, errors = 0, frames = 0, images = 0,
               imageBytes = 0;
        for (const auto &file : std::filesystem::recursive_directory_iterator(root / "data"))
            if (file.is_regular_file()) {
                auto ext = file.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                               [](unsigned char c) { return char(std::tolower(c)); });
                try {
                    if (ext == ".ply") {
                        auto c = Chart::parse(readFile(file.path().string()));
                        ++charts;
                        objects += c.objects.size();
                    } else if (ext == ".ifs") {
                        IfsArchive a(readFile(file.path().string()));
                        for (const auto &e : a.entries())
                            a.read(e.path);
                        for (const auto &t : textureList(a)) {
                            imageBytes += texturePixels(a, t).size();
                            ++images;
                        }
                        ++ifs;
                    } else if (ext == ".sd9" || ext == ".2dx") {
                        auto w = Wave::parse(readFile(file.path().string()));
                        for (size_t i = 0; i < w.blockCount(); ++i) {
                            auto block = w.decodeBlock(i);
                            frames += block.size() / 2;
                        }
                        ++audio;
                    }
                } catch (const std::exception &e) {
                    ++errors;
                    std::cerr << file.path().string() << ": " << e.what() << '\n';
                }
            }
        auto catalog = readCatalog(readFile((root / "modules/reflecbeat.dll").string()));
        std::cout << "{\"charts\":" << charts << ",\"objects\":" << objects << ",\"ifs\":" << ifs
                  << ",\"audio\":" << audio << ",\"decodedFrames\":" << frames
                  << ",\"catalog\":" << catalog.size() << ",\"textures\":" << images
                  << ",\"textureBytes\":" << imageBytes << ",\"errors\":" << errors << "}\n";
        return errors ? 2 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
