#include "rb/OriginalAfpMoviePool.hpp"
#include <iostream>
using namespace rb;
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--table") {
            for (const auto &d : OriginalAfpMoviePool::descriptors())
                std::cout << d.poolIndex << ' ' << d.capacity << ' ' << d.package << ' ' << d.layer
                          << ' ' << d.creationOption << ' ' << d.movieName << ' '
                          << d.alternativeMovieName << ' ' << d.absolutePriority << '\n';
            return 0;
        }
        unsigned checks = 0;
        auto check = [&](bool good) {
            if (!good)
                throw Error("AFP pool test");
            ++checks;
        };
        OriginalAfpMoviePool pools;
        for (const auto &d : OriginalAfpMoviePool::descriptors()) {
            for (uint32_t i = 0; i < d.capacity; ++i)
                check(pools.acquire(d.poolIndex) == i);
            check(!pools.acquire(d.poolIndex));
            auto &s = pools.state(d.poolIndex, 0);
            s.label = "on";
            s.labelTick = -39;
            s.layerPlaying = true;
            s.visible = true;
            pools.release(d.poolIndex, 0);
            check(!s.visible && s.layerPlaying && s.labelTick == -39);
            pools.release(d.poolIndex, d.capacity - 1);
            check(pools.acquire(d.poolIndex) == d.capacity - 1);
            check(pools.acquire(d.poolIndex) == 0);
            check(&pools.state(d.poolIndex, 0) == &s && s.label == "on" && s.labelTick == -39);
        }
        bool bad = false;
        try {
            pools.release(23, 64);
        } catch (const Error &) {
            bad = true;
        }
        check(bad);
        bad = false;
        try {
            pools.acquire(37);
        } catch (const Error &) {
            bad = true;
        }
        check(bad);
        std::cout << "PASS OriginalAfpMoviePool " << checks << " checks\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
