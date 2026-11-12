#pragma once
#include "rb/AvsBootConfig.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rb {
// Owns the recovered startup prefix on one real native thread. It does not
// manufacture a completed AVS boot or dispatch an unrecovered game scene.
class AvsNativeRuntime {
public:
    enum class Stage { descriptors, platform, tasks, workers, mutexes,
        synchronization, alarms, descriptorService, mainThread, filesystem };
    struct Report {
        Stage stage = Stage::descriptors;
        bool threadPrefixReady = false;
        bool failed = false;
        std::string message;
        std::vector<std::string> trace;
        uint64_t originalArenaBytes = 0;
        uint32_t mainDescriptor = 0, descriptorMutex = 0, initialWorkers = 0;
        uint32_t processors = 0, spinCount = 0;
        bool alarmEntered = false, alarmPriorityApplied = false;
    };
    // A stage observer supports deterministic allocation/cancellation fault
    // tests. It runs before the named stage and cannot declare stage success.
    using BeforeStage = std::function<void(Stage)>;
    AvsNativeRuntime();
    ~AvsNativeRuntime();
    AvsNativeRuntime(const AvsNativeRuntime &) = delete;
    AvsNativeRuntime &operator=(const AvsNativeRuntime &) = delete;
    Report start(const AvsBootConfigImage &, uint32_t processAffinityMask,
                 uint64_t avsHeapBytes, BeforeStage = {});
    void requestStop();
    void stop();
    Report report() const;
    static const char *address(Stage) noexcept;
    // Apple exposes an online processor count, not a Windows affinity mask.
    // The adapter represents that count as the original 32-bit low-bit mask.
    static uint32_t nativeProcessAffinityMask();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rb
