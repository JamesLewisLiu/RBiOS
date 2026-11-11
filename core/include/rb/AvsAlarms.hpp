#pragma once
#include "AvsDescriptors.hpp"
#include <functional>
#include <memory>
namespace rb {
class AvsAlarms {
public:
    using Clock = std::function<uint32_t()>;
    using Callback = std::function<bool(uint64_t)>;
    struct RecordState { uint64_t deadline, period; bool allocated, queued, retired, automatic; };
    struct State { std::vector<RecordState> records; std::vector<size_t> queue, free; uint64_t minimum; size_t used, highWater; uint32_t pendingWakeSignals; };
    // semaphoreLimit comes from the original thread boot's nr_thread setting.
    AvsAlarms(AvsDescriptors &, uint16_t capacity, uint32_t semaphoreLimit, Clock clock = {});
    ~AvsAlarms();
    AvsAlarms(const AvsAlarms &) = delete;
    AvsAlarms &operator=(const AvsAlarms &) = delete;
    int32_t create(Callback);
    int32_t destroy(uint32_t);
    // Original return is prior-active state, not a success flag.
    int32_t set(uint32_t, uint64_t milliseconds);
    // Original set_once retains an existing schedule; it is not one-shot.
    int32_t setOnce(uint32_t, uint64_t milliseconds);
    bool createProcedure(uint64_t milliseconds, Callback);
    // Runs on the original alarm task's admitted native worker. Caller owns
    // reserved 0x40003 task identity/TLS and must join before destruction.
    void run();
    void requestStop();
    bool running() const;
    // Deterministic offline dispatch, forbidden while run() owns the service.
    size_t poll();
    State state() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    static int32_t dispose(uintptr_t);
};
} // namespace rb
