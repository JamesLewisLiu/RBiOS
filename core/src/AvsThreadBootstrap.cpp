#include "rb/AvsThreadBootstrap.hpp"
#include <stdexcept>

namespace rb {
namespace {
uint32_t read(const AvsBootConfigImage &image, size_t offset, size_t bytes) {
    uint32_t value = 0;
    for (size_t i = 0; i < bytes; ++i) value |= uint32_t(image[offset+i]) << (8*i);
    return value;
}
uint32_t page(uint32_t value) { return (value + 4095u) & 0xfffff000u; }
}
uint32_t avsProcessProcessorCount(uint32_t mask) noexcept {
    uint32_t count = 0; while (mask) { count += mask & 1; mask >>= 1; } return count;
}
AvsThreadBootstrapPlan planAvsThreadBootstrap(const AvsBootConfigImage &image, uint32_t processMask) {
    AvsThreadBootstrapPlan plan;
    plan.processorCount = avsProcessProcessorCount(processMask);
    plan.spinCount = plan.processorCount >= 2 ? 4000 : 0;
    plan.threadCount = read(image,104,2);
    plan.semaphoreLimit = plan.threadCount;
    if (!plan.threadCount) throw std::invalid_argument("AVS thread boot: nr_thread is zero");
    plan.minimumStack = page(read(image,96,4));
    plan.maximumStack = page(read(image,100,4));
    if (!plan.minimumStack || !plan.maximumStack || plan.minimumStack > plan.maximumStack)
        throw std::invalid_argument("AVS thread boot: invalid rounded stack limits");
    plan.initialCount = read(image,88,2);
    if (plan.initialCount > plan.threadCount) {
        plan.initialCount = plan.threadCount;
        plan.warnings.push_back(AvsThreadBootWarning::initialCountClamped);
    }
    plan.expiryCount = read(image,84,4);
    if (plan.expiryCount == 1) plan.warnings.push_back(AvsThreadBootWarning::expiryDisablesPool);
    // Original CPU discovery is 32-bit; the mask below uses CPU count, not
    // original process-mask bit positions. Preserve that distinction.
    const uint64_t countMask = plan.processorCount == 32 ? UINT64_C(0xffffffff)
        : (UINT64_C(1) << plan.processorCount) - 1;
    plan.affinityMask = (uint64_t(read(image,76,4)) << 32 | read(image,72,4)) & countMask;
    plan.workerStack = page(read(image,80,4));
    if (plan.workerStack && (((plan.maximumStack - plan.workerStack) |
        (plan.workerStack - plan.minimumStack)) & 0x80000000u)) {
        if (plan.workerStack >= plan.minimumStack) {
            plan.workerStack = plan.maximumStack;
            plan.warnings.push_back(AvsThreadBootWarning::stackReduced);
        } else {
            plan.workerStack = plan.minimumStack;
            plan.warnings.push_back(AvsThreadBootWarning::stackRaised);
        }
    }
    plan.taskArenaBytes = (plan.threadCount * 48u + 51u) & ~15u;
    plan.workerArenaSuppliedBytes = plan.threadCount * 72u + 331u;
    plan.workerArenaConsumedBytes = plan.threadCount * 72u + 316u;
    return plan;
}
AvsThreadAffinity::AvsThreadAffinity(uint64_t mask) {
    int first = -1, previous = -1;
    for (int bit = 63; bit >= 0; --bit) if (mask & (UINT64_C(1) << bit)) {
        if (first < 0) first = bit;
        else { nodes_[previous].next = int8_t(bit); nodes_[bit].previous = int8_t(previous); }
        previous = bit;
    }
    head_ = previous;
    if (previous >= 0) { nodes_[previous].next = int8_t(first); nodes_[first].previous = int8_t(previous); }
}
int32_t AvsThreadAffinity::next() {
    const auto selected = head_;
    if (selected < 0) return selected;
    auto &node = nodes_[selected];
    const auto count = ++node.count;
    const auto next = nodes_[head_].next;
    if (selected == next) return selected;
    auto previous = node.previous;
    if (count < nodes_[previous].count) return selected;
    head_ = previous;
    if (count >= nodes_[next].count) return selected;
    // 1005C0E0 reinsertion branch after counter wrap/unequal usage.
    auto successor = node.next;
    nodes_[previous].next = successor;
    nodes_[successor].previous = previous;
    do { successor = previous; previous = nodes_[previous].previous; }
    while (count >= nodes_[previous].count && previous != head_);
    node.next = successor; node.previous = previous;
    nodes_[successor].previous = int8_t(selected); nodes_[previous].next = int8_t(selected);
    return selected;
}
} // namespace rb
