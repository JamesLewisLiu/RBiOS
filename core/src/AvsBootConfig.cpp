#include "rb/AvsBootConfig.hpp"
#include <cstring>
namespace rb {
std::vector<AvsProductFix> applyAvsProductMode(AvsBootConfigImage &config, bool productMode) {
    std::vector<AvsProductFix> events;
    if (!productMode) return events;
    if (config[1]) {
        events.push_back({AvsProductFix::Field::netSciEnable, config[1]});
        config[1] = 0;
    }
    const uint16_t delay = uint16_t(config[118]) | uint16_t(config[119]) << 8;
    if (delay) {
        events.push_back({AvsProductFix::Field::mutexDelay, delay});
        config[118] = config[119] = 0;
    }
    uint32_t bits = 0;
    for (unsigned i = 0; i < 4; ++i) bits |= uint32_t(config[2268 + i]) << (8 * i);
    if (bits) {
        int32_t offset; std::memcpy(&offset, &bits, sizeof(offset));
        events.push_back({AvsProductFix::Field::debugLocalOffset, offset});
        for (unsigned i = 0; i < 4; ++i) config[2268 + i] = 0;
    }
    return events;
}
} // namespace rb
