#pragma once
#include "rb/AvsDescriptors.hpp"
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>
namespace rb {
class AvsResourcePool {
public:
    struct Statistics { uint32_t capacity, peakActive, active; };
    struct Layout {
        uint32_t count, stride, headerOffset, firstPayloadOffset;
        uint64_t arenaBytes;
    };
    // Original 1000F900 byte layout relative to a four-byte-aligned address.
    static Layout layout(uintptr_t arenaAddress, uint32_t elementSize, uint32_t count, uint32_t alignment = 8);
    // Exact arena advance used by 10039D20 (alignment eight).
    static uint64_t wrapperArenaBytes(uint32_t elementSize, uint32_t count);
    AvsResourcePool(AvsDescriptors &, uint32_t elementSize, uint32_t count, uint32_t alignment = 8);
    ~AvsResourcePool();
    AvsResourcePool(const AvsResourcePool &) = delete;
    AvsResourcePool &operator=(const AvsResourcePool &) = delete;
    void *allocate();
    uint32_t release(void *);
    // Reset invalidates all allocations without destroying native objects.
    // Callers must destroy constructed objects first; reset does not zero data.
    void reset();
    uint32_t descriptor() const noexcept { return descriptor_; }
    uint32_t capacity() const noexcept { return count_; }
    uint32_t elementSize() const noexcept { return elementSize_; }
    uint32_t payloadSize() const noexcept { return stride_ - 8; }
    uint32_t stride() const noexcept { return stride_; }
    uint32_t freeCount() const;
    Statistics statistics() const;
    uint32_t indexOf(const void *) const;
    void *at(uint32_t index, bool allocatedOnly = false) const;
    bool allocated(const void *) const;
    std::vector<uint32_t> activeIndices() const;
    // Original query opcodes 9,10,11,12 share one cursor per pool.
    void *firstActive();
    void *lastActive();
    void *nextActive();
    void *previousActive();
private:
    struct Slot { uint32_t previous, next; };
    static constexpr uint32_t none = UINT32_MAX, freeSlot = UINT32_MAX-1;
    bool validDescriptor() const;
    uint32_t indexUnchecked(const void *) const;
    void *pointer(uint32_t) const;
    void initialize();
    AvsDescriptors &descriptors_;
    uint32_t descriptor_ = 0, elementSize_, count_, stride_, alignment_;
    std::byte *arena_ = nullptr, *payload_ = nullptr;
    std::vector<Slot> slots_;
    uint32_t head_ = none, tail_ = none, free_ = 0, cursor_ = none, available_ = 0, minimumFree_ = 0;
    mutable std::recursive_mutex mutex_;
};
}
