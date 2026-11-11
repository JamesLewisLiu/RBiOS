#pragma once
#include "rb/AvsBootConfig.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace rb {
enum class AvsThreadBootWarning { initialCountClamped, expiryDisablesPool, stackRaised, stackReduced };
struct AvsThreadBootstrapPlan {
    uint32_t processorCount = 0, spinCount = 0, semaphoreLimit = 0;
    uint32_t minimumStack = 0, maximumStack = 0;
    uint64_t affinityMask = 0;
    uint32_t threadCount = 0, initialCount = 0, expiryCount = 0, workerStack = 0;
    uint32_t workerPriority = 2;
    uint32_t taskArenaBytes = 0, workerArenaSuppliedBytes = 0, workerArenaConsumedBytes = 0;
    std::vector<AvsThreadBootWarning> warnings;
};
// Original 10039570 counts bits in the 32-bit process mask, not the system mask.
uint32_t avsProcessProcessorCount(uint32_t processAffinityMask) noexcept;
// 10039530 + 1001ABC0 transformations. This creates an explicit plan, not an
// allocated worker service and not a successful bootstrap-stage result.
AvsThreadBootstrapPlan planAvsThreadBootstrap(const AvsBootConfigImage &, uint32_t processAffinityMask);
// Original 1005C1D0/1005C1A0 selection state, initialized from a zeroed pool.
class AvsThreadAffinity {
public:
    explicit AvsThreadAffinity(uint64_t mask);
    int32_t next();
    void release(int32_t selected);
private:
    struct Node { uint16_t count = 0; int8_t next = -1, previous = -1; };
    std::array<Node, 64> nodes_{};
    int32_t head_ = -1;
};
} // namespace rb
