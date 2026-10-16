#pragma once
#include "AfpTimeline.hpp"
#include <optional>

namespace rb {
struct OriginalAfpPoolDescriptor {
    uint32_t poolIndex, capacity, package, layer, creationOption;
    const char *movieName, *alternativeMovieName;
    bool absolutePriority;
};
// Original 1036BFD8 descriptors and 1001EF40 / 10164C30 / 10164BC0 free-list rules.
// Own one instance per stage. Movie clocks survive object release/reacquisition.
class OriginalAfpMoviePool {
    struct Slot {
        SwitchMovieState state;
        uint32_t next = UINT32_MAX;
        bool acquired = false;
    };
    struct Pool {
        std::vector<Slot> slots;
        uint32_t head = UINT32_MAX;
    };
    std::array<Pool, 37> pools_;

  public:
    OriginalAfpMoviePool();
    static const std::array<OriginalAfpPoolDescriptor, 37> &descriptors();
    static const OriginalAfpPoolDescriptor &descriptor(uint32_t poolId);
    std::optional<uint32_t> acquire(uint32_t poolId); // nullopt on original pool exhaustion
    void release(uint32_t poolId, uint32_t slot);
    SwitchMovieState &state(uint32_t poolId, uint32_t slot);
};
} // namespace rb
