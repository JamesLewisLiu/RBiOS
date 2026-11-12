#pragma once
#include "rb/AvsNativeWorkerPool.hpp"
#include "rb/AvsResourcePool.hpp"
#include "rb/AvsTaskCompletionRegistry.hpp"
#include <functional>
#include <mutex>
#include <vector>

namespace rb {
// Original export 091 / 1001A7D0 task admission, using the real recovered pools.
class AvsTasks {
public:
    using Procedure = int32_t (*)(uintptr_t);
    static constexpr int32_t invalidArgument = int32_t(0x80010016u);
    static constexpr int32_t creationFailed = int32_t(0x8001000cu);
    enum class Failure { none, arguments, taskPool, control, worker, descriptor, dispatch };
    AvsTasks(AvsDescriptors &, AvsNativeWorkerPool &, const AvsThreadBootstrapPlan &);
    AvsTasks(AvsDescriptors &, AvsNativeWorkerPool &, const AvsThreadBootstrapPlan &, AvsResourcePool &,
             AvsTaskCompletionRegistry * = nullptr);
    static uint32_t recordSize() noexcept;
    static uint32_t recordAlignment() noexcept;
    ~AvsTasks();
    int32_t create(Procedure, uintptr_t argument, uint32_t stackSize, uint32_t priority);
    int32_t wait(uint32_t descriptor);
    int32_t destroy(uint32_t descriptor);
    Failure lastFailure() const;
    uint32_t freeCount() const;
    uint32_t poolDescriptor() const noexcept;
    AvsTaskCompletionRegistry &completions() noexcept {return *registry_;}
private:
    struct Record;
    static int32_t dispose(uintptr_t payload);
    Record &record(uint32_t token);
    void releaseRecord(Record &);
    AvsDescriptors &descriptors_;
    AvsNativeWorkerPool &workers_;
    AvsThreadBootstrapPlan plan_;
    std::unique_ptr<AvsResourcePool> ownedPool_;
    AvsResourcePool &pool_;
    std::unique_ptr<AvsTaskCompletionRegistry> ownedRegistry_;
    AvsTaskCompletionRegistry *registry_;
    mutable std::recursive_mutex mutex_;
    std::vector<Record *> records_;
    // Last binding per descriptor index, retained across deferred bulk cleanup.
    std::vector<std::pair<uint32_t,uintptr_t>> bindings_;
    Failure failure_ = Failure::none;
};
} // namespace rb
