#pragma once
#include "rb/AvsAlarms.hpp"
#include "rb/AvsMutexes.hpp"
#include <memory>

namespace rb {
// Private 10040700 alarm task. Dependencies must outlive this owner. The
// reserved token is bound/neutralized, never allocated from the public pool.
class AvsReservedAlarmTask {
public:
    static constexpr uint32_t descriptor = 0x40003;
    struct Status {
        bool nativeStarted, taskEntered, completed, joined, stopRequested, failed;
        int32_t result;
        uintptr_t workerIdentity;
        uint32_t requestedStack, priority, remainingUses;
        int32_t affinity;
        bool priorityApplied;
    };
    AvsReservedAlarmTask(AvsDescriptors &, AvsMutexes &, AvsAlarms &);
    ~AvsReservedAlarmTask();
    AvsReservedAlarmTask(const AvsReservedAlarmTask &) = delete;
    AvsReservedAlarmTask &operator=(const AvsReservedAlarmTask &) = delete;
    void start();
    void requestStop();
    int32_t wait(); // Actual completion; does not request shutdown.
    void stop(); // Request shutdown, wait, join, and detach the reserved binding.
    Status status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    static int32_t dispose(uintptr_t);
};
} // namespace rb
