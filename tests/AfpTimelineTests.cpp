#include "rb/AfpTimeline.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
using namespace rb;
int main(int argc, char **argv) {
    try {
        size_t checks = 0;
        auto expect = [&](bool v) {
            if (!v)
                throw Error("AFP timeline check");
            ++checks;
        };
        bool bad = false;
        try {
            AfpTimeline t(Bytes{}, Bytes{});
        } catch (const Error &) {
            bad = true;
        }
        expect(bad);
        if (argc == 3 &&
            (std::string(argv[1]) == "--sample" || std::string(argv[1]) == "--label")) {
            AfpPackage p(readFile(argv[2]));
            std::string name, label;
            uint32_t frame;
            std::map<std::string, std::unique_ptr<AfpTimeline>> movies;
            while (std::cin >> name) {
                const bool labeled = std::string(argv[1]) == "--label";
                if (labeled)
                    std::cin >> label;
                std::cin >> frame;
                auto &ptr = movies[name];
                if (!ptr)
                    ptr = std::make_unique<AfpTimeline>(p.movieBytes(name), p.bitStreamBytes(name));
                auto d = labeled ? ptr->sampleExportedLabelFrame(p, name, label, frame)
                                 : ptr->sampleExportedFrame(p, name, frame);
                std::cout << d.size() << std::setprecision(9);
                for (const auto &v : d) {
                    std::cout << ' ' << v.texture;
                    for (float x :
                         {v.matrix.a, v.matrix.b, v.matrix.c, v.matrix.d, v.matrix.tx, v.matrix.ty})
                        std::cout << ' ' << x;
                    for (auto x : v.bounds)
                        std::cout << ' ' << x;
                    for (auto x : v.multiply)
                        std::cout << ' ' << x;
                    std::cout << ' ' << unsigned(v.blend);
                }
                std::cout << '\n';
            }
            return 0;
        }
        if (argc == 2) {
            AfpPackage p(readFile(argv[1]));
            for (const auto &name : {"obj_bigbang_big_red", "obj_bigbang_big_blue",
                                     "obj_bigbang_small_red", "obj_bigbang_small_blue"}) {
                AfpTimeline t(p.movieBytes(name), p.bitStreamBytes(name));
                expect(t.fps == 60);
                expect(t.root->frames.size() == 120);
                for (unsigned f = 0; f < 120; ++f) {
                    auto d = t.sampleExportedFrame(p, name, f);
                    expect(d.size() == 2);
                    for (const auto &v : d)
                        expect(!v.texture.empty());
                }
            }
            for (const auto &name : {"swich_on", "swich_off"}) {
                AfpTimeline t(p.movieBytes(name), p.bitStreamBytes(name));
                expect(t.root->labels.at("off") == 1);
                expect(t.root->labels.at("on") == 4);
                expect(t.root->labels.at("loop") == 5);
                for (unsigned f = 0; f < 60; ++f) {
                    const auto draws = t.sampleExportedFrame(p, name, f);
                    expect(!draws.empty());
                    for (const auto &draw : draws)
                        for (float c : draw.addition)
                            expect(c == 0);
                }
                const auto same = [&](const auto &a, const auto &b) {
                    if (a.size() != b.size())
                        return false;
                    for (size_t i = 0; i < a.size(); ++i)
                        if (a[i].texture != b[i].texture || a[i].bounds != b[i].bounds ||
                            a[i].multiply != b[i].multiply || a[i].matrix.tx != b[i].matrix.tx ||
                            a[i].matrix.ty != b[i].matrix.ty)
                            return false;
                    return true;
                };
                expect(same(t.sampleExportedLabelFrame(p, name, "on", 0),
                            t.sampleExportedFrame(p, name, 3)));
                expect(same(t.sampleExportedLabelFrame(p, name, "on", 1),
                            t.sampleExportedFrame(p, name, 4)));
                expect(same(t.sampleExportedLabelFrame(p, name, "on", 42),
                            t.sampleExportedFrame(p, name, 4)));
                expect(same(t.sampleExportedLabelFrame(p, name, "on", 83),
                            t.sampleExportedFrame(p, name, 4)));
                expect(same(t.sampleExportedLabelFrame(p, name, "off", 10000),
                            t.sampleExportedFrame(p, name, 2)));
                bool rejected = false;
                try {
                    t.sampleExportedLabelFrame(p, name, "unknown", 0);
                } catch (const Error &) {
                    rejected = true;
                }
                expect(rejected);
                SwitchMovieState state;
                expect(!state.layerPlaying && !state.visible && state.label.empty());
                auto sample = prepareSwitchMovieDraw(t, name, state, 10, false);
                expect(sample.label == "off" && sample.elapsed == 0 && state.labelTick == 10);
                sample = prepareSwitchMovieDraw(t, name, state, 15, true);
                expect(sample.label == "on" && sample.elapsed == 0 && sample.frame == 3);
                state.visible = false; // original hides without pausing; other resource selected
                sample = prepareSwitchMovieDraw(t, name, state, 30, true);
                expect(sample.label == "on" && sample.elapsed == 15 && state.labelTick == 15);
                sample = prepareSwitchMovieDraw(t, name, state, 31, false);
                expect(sample.label == "off" && sample.elapsed == 0 && state.labelTick == 31);
                sample = prepareSwitchMovieDraw(t, name, state, 32, false);
                expect(sample.elapsed == 0 && state.labelTick == 32); // original repeats off seek
                sample = prepareSwitchMovieDraw(t, name, state, 33, true);
                expect(sample.label == "on" && sample.elapsed == 0);
                state.visible = false; // return to pool and reacquire the same movie instance
                sample = prepareSwitchMovieDraw(t, name, state, 60, true);
                expect(sample.elapsed == 27 && state.labelTick == 33);
                // A genuinely unsupported action must fail, rather than silently appear static.
                auto &definition =
                    *std::find_if(t.root->tags.begin(), t.root->tags.end(),
                                  [&](const auto &tag) { return tag.id == t.exports.at(name); });
                auto &action =
                    *std::find_if(definition.sprite->tags.begin(), definition.sprite->tags.end(),
                                  [](const auto &tag) { return tag.kind == 0x7a; });
                const auto saved = action.payload.front();
                action.payload.front() = 0;
                rejected = false;
                try {
                    t.sampleExportedFrame(p, name, 4);
                } catch (const Error &) {
                    rejected = true;
                }
                action.payload.front() = saved;
                expect(rejected);
            }
        }
        std::cout << "PASS AfpTimeline " << checks << " checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
