#pragma once
#include <cstdint>

namespace rb {
// Boundary used by original TEST wait state 100D5710. A concrete EA3 adapter
// must implement both operations; there is deliberately no ready default.
class OriginalTestReadinessSource {
public:
    virtual ~OriginalTestReadinessSource() = default;
    virtual void beginTestWait() = 0;             // XE592acd00005D
    virtual int32_t readTestWaitStatus() = 0;     // XE592acd000070
};

struct OriginalTestReadinessDecision {
    int32_t normalizedStatus;
    // Queues 100D5600, zeros next-state counters (+164/+172), and sets +176.
    // This is NOT proof of successful EA3 initialization: errors queue it too.
    bool queueMainMenu;
};

// Original 100A1610, including its -16 fallback for unknown values.
int32_t normalizeOriginalTestStatus(int32_t status) noexcept;
// Original stateTick is the signed counter at scene offset +180. The scene
// owner advances it; repeated zero ticks repeat beginTestWait, as in the DLL.
// The caller must apply a queued transition through the original scene owner.
OriginalTestReadinessDecision pollOriginalTestReadiness(
    int32_t stateTick, OriginalTestReadinessSource &source);
} // namespace rb
