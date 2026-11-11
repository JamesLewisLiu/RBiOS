#include "rb/OriginalTestReadiness.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
struct Source final : rb::OriginalTestReadinessSource {
    explicit Source(int32_t value) : status(value) {}
    int32_t status;
    std::string calls;
    void beginTestWait() override { calls += 'B'; }
    int32_t readTestWaitStatus() override { calls += 'S'; return status; }
};
void require(bool condition) {
    if (!condition) throw std::runtime_error("original TEST readiness regression");
}
}

int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        int32_t tick, status;
        while (std::cin >> tick >> status) {
            Source source(status);
            const auto result = rb::pollOriginalTestReadiness(tick, source);
            std::cout << result.normalizedStatus << ' ' << result.queueMainMenu
                      << ' ' << source.calls << '\n';
        }
        return 0;
    }
    for (int32_t status = -24; status <= 3; ++status) {
        for (int32_t tick : {0, 1, -1, std::numeric_limits<int32_t>::min(),
                             std::numeric_limits<int32_t>::max()}) {
            Source source(status);
            const auto result = rb::pollOriginalTestReadiness(tick, source);
            require(result.normalizedStatus == (status >= -22 && status <= 1 ? status : -16));
            require(result.queueMainMenu == (status != 1));
            require(source.calls == (tick == 0 ? "BS" : "S"));
        }
    }
    for (int32_t status : {std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()})
        require(rb::normalizeOriginalTestStatus(status) == -16);
    Source sequence(1);
    require(!rb::pollOriginalTestReadiness(0, sequence).queueMainMenu);
    require(!rb::pollOriginalTestReadiness(0, sequence).queueMainMenu);
    sequence.status = -5;
    require(rb::pollOriginalTestReadiness(2, sequence).queueMainMenu);
    require(sequence.calls == "BSBSS");
    std::cout << "PASS original TEST wait status, call order and error transition\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n'; return 1;
}}
