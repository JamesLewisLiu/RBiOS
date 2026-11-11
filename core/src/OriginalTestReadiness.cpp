#include "rb/OriginalTestReadiness.hpp"

namespace rb {
int32_t normalizeOriginalTestStatus(int32_t status) noexcept {
    // The original switch explicitly handles -22..-17 and -15..1. Its
    // default is -16, so this interval is equivalent without signed overflow.
    return status >= -22 && status <= 1 ? status : -16;
}

OriginalTestReadinessDecision pollOriginalTestReadiness(
    int32_t stateTick, OriginalTestReadinessSource &source) {
    if (stateTick == 0) source.beginTestWait();
    const int32_t status = normalizeOriginalTestStatus(source.readTestWaitStatus());
    return {status, status != 1};
}
} // namespace rb
