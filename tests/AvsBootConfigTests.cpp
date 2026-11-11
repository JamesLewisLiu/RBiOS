#include "rb/AvsBootConfig.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rb;
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        unsigned enabled; std::string hex;
        while (std::cin >> enabled >> hex) {
            AvsBootConfigImage config{};
            if (hex.size() != config.size() * 2) throw std::runtime_error("config image size");
            for (size_t i = 0; i < config.size(); ++i) config[i] = uint8_t(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
            const auto events = applyAvsProductMode(config, enabled != 0);
            unsigned mask = 0; for (const auto &e : events) mask |= 1u << unsigned(e.field);
            std::cout << mask << ' ';
            constexpr char digits[] = "0123456789abcdef";
            for (auto byte : config) std::cout << digits[byte >> 4] << digits[byte & 15];
            for (const auto &e : events) std::cout << ' ' << unsigned(e.field) << ':' << e.previous;
            std::cout << '\n';
        }
        return 0;
    }
    for (unsigned flags = 0; flags < 8; ++flags) {
        AvsBootConfigImage before; before.fill(0xA5);
        before[1] = flags & 1 ? 255 : 0;
        before[118] = before[119] = flags & 2 ? 255 : 0;
        for (unsigned i = 0; i < 4; ++i) before[2268+i] = flags & 4 ? 255 : 0;
        auto untouched = before;
        if (!applyAvsProductMode(untouched, false).empty() || untouched != before) throw std::runtime_error("non-product mutation");
        auto after = before; const auto events = applyAvsProductMode(after, true);
        unsigned mask = 0;
        for (const auto &e : events) {
            mask |= 1u << unsigned(e.field);
            const int32_t expected[] = {255, 65535, -1};
            if (e.previous != expected[unsigned(e.field)]) throw std::runtime_error("wrong log value");
        }
        if (mask != flags) throw std::runtime_error("missing original log event");
        for (size_t i = 0; i < after.size(); ++i) {
            const bool changed = i == 1 || i == 118 || i == 119 || (i >= 2268 && i < 2272);
            if (after[i] != (changed ? 0 : before[i])) throw std::runtime_error("unrelated configuration changed");
        }
        if (!applyAvsProductMode(after, true).empty()) throw std::runtime_error("duplicate correction logs");
    }
    std::cout << "PASS AVS product-mode fields, log order/values, unrelated bytes and disabled mode\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
