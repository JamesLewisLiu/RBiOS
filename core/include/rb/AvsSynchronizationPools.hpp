#pragma once
#include "rb/AvsBootConfig.hpp"
#include "rb/AvsResourcePool.hpp"
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
namespace rb {
// Original 10029410 passive allocation stages after mutex pool creation.
// Owning a pool does not implement condition/event/semaphore operations.
class AvsSynchronizationPools {
public:
    enum class Kind { condition, event, semaphore, conditionState };
    struct PoolState {
        Kind kind;
        uint16_t capacity;
        uint32_t elementSize, descriptor;
        uint64_t originalArenaOffset, originalArenaBytes;
    };
    class InitializationError : public std::runtime_error {
    public:
        InitializationError(Kind stage, const std::string &reason);
        Kind stage() const noexcept { return stage_; }
    private:
        Kind stage_;
    };
    AvsSynchronizationPools(AvsDescriptors &, const AvsBootConfigImage &);
    AvsSynchronizationPools(const AvsSynchronizationPools &) = delete;
    AvsSynchronizationPools &operator=(const AvsSynchronizationPools &) = delete;
    AvsResourcePool &condition() noexcept { return *pools_[0]; }
    AvsResourcePool &event() noexcept { return *pools_[1]; }
    AvsResourcePool &semaphore() noexcept { return *pools_[2]; }
    AvsResourcePool &conditionState() noexcept { return *pools_[3]; }
    const std::array<PoolState,4> &state() const noexcept { return state_; }
    uint64_t originalArenaBytes() const noexcept { return arenaBytes_; }
private:
    std::array<PoolState,4> state_{};
    std::array<std::unique_ptr<AvsResourcePool>,4> pools_;
    uint64_t arenaBytes_ = 0;
};
}
