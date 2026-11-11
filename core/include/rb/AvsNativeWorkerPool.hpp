#pragma once
#include "rb/AvsMutexes.hpp"
#include "rb/AvsThreadBootstrap.hpp"
#include <array>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <vector>

namespace rb {
// Concrete initial-worker ownership for 10041E40 and the 10041660 entry loop.
// Task selection/queue admission is deliberately separate; assign targets an
// already selected original worker slot, not a replacement scheduling policy.
class AvsNativeWorkerPool {
public:
    using Task = std::function<int32_t()>;
    using Completion = std::array<std::function<void()>, 4>;
    struct WorkerStatus {
        uint16_t state;
        uint32_t remainingUses;
        int32_t affinity;
        bool affinityApplied;
        bool started;
        bool failed;
        uint32_t stackSize, priority;
        bool priorityApplied;
    };
    AvsNativeWorkerPool(const AvsThreadBootstrapPlan &, AvsMutexes &);
    ~AvsNativeWorkerPool();
    AvsNativeWorkerPool(const AvsNativeWorkerPool &) = delete;
    AvsNativeWorkerPool &operator=(const AvsNativeWorkerPool &) = delete;
    std::future<int32_t> assign(size_t slot, Task, Completion = {});
    std::optional<size_t> checkout(uint32_t stackSize, uint32_t priority, uint32_t expiry = 1);
    void waitIdle(size_t slot);
    WorkerStatus status(size_t slot) const;
    const std::vector<size_t> &initialStartOrder() const noexcept;
    void stop();
    static uintptr_t currentWorkerIdentity() noexcept;
    bool isCurrentWorker(size_t slot) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rb
