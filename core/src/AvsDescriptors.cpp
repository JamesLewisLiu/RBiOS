#include "rb/AvsDescriptors.hpp"
#include <stdexcept>

namespace rb {
AvsDescriptors::AvsDescriptors(uint32_t capacity) {
    // The original assumes a supplied buffer and a positive, encodable count.
    if (capacity == 0 || capacity > 0x1fffc)
        throw std::invalid_argument("AVS descriptor capacity is outside the recovered domain");
    entries_.resize(capacity + 4);
    for (auto &group : heads_) group.fill(none);
    for (uint32_t index = 0; index < entries_.size(); ++index)
        entries_[index].token = index;
    for (uint32_t index = 0; index < 3; ++index) {
        entries_[index].token |= 12u << 17;
        link(index, 12);
    }
    entries_[3].token |= 2u << 17;
    link(3, 2);
    for (uint32_t index = 4; index + 1 < entries_.size(); ++index)
        entries_[index].next = index + 1;
}

void AvsDescriptors::link(uint32_t index, uint32_t type) {
    auto &entry = entries_[index];
    entry.previous = none;
    entry.next = heads_[entry.group][type];
    if (entry.next != none) entries_[entry.next].previous = index;
    heads_[entry.group][type] = index;
}

AvsDescriptors::Entry *AvsDescriptors::find(uint32_t token) {
    const auto index = token & 0x1ffff;
    return index < entries_.size() && entries_[index].token == token ? &entries_[index] : nullptr;
}
const AvsDescriptors::Entry *AvsDescriptors::find(uint32_t token) const {
    const auto index = token & 0x1ffff;
    return index < entries_.size() && entries_[index].token == token ? &entries_[index] : nullptr;
}

uint32_t AvsDescriptors::allocate(uint32_t type, Close close, uintptr_t payload) {
    type &= 63;
    if (type == 0) throw std::invalid_argument("AVS descriptor type zero precedes the original category table");
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (free_ == none) return 0;
    if (++generation_ == 0) generation_ = 1;
    const auto index = free_;
    auto &entry = entries_[index];
    free_ = entry.next;
    entry.token = (index & 0x1ffff) | (type << 17) | ((generation_ & 255) << 23);
    entry.payload = payload;
    entry.close = close ? close : noClose;
    entry.group = uint8_t(group_);
    entry.deferred = false;
    link(index, type);
    return entry.token;
}

bool AvsDescriptors::lookup(uint32_t token, uintptr_t *payload) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto *entry = find(token);
    if (!entry) return false;
    if (payload) *payload = entry->payload;
    return true;
}

bool AvsDescriptors::replace(uint32_t token, Close close, uintptr_t payload) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (auto *entry = find(token)) {
        entry->payload = payload;
        entry->close = close ? close : noClose;
        return true;
    }
    return false;
}

int32_t AvsDescriptors::release(uint32_t token) {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    auto *entry = find(token);
    if (!entry) return 0;
    if (entry->deferred) return 0;
    // Original reserved records must receive their callbacks from their owner.
    if (!entry->close) throw std::logic_error("AVS descriptor owner has not installed its callback");
    const auto index = token & 0x1ffff;
    const auto type = (token >> 17) & 63;
    if (type == 0) throw std::logic_error("AVS descriptor is already on the free list");
    const auto callback = entry->close;
    const auto payload = entry->payload;
    if (entry->previous != none) entries_[entry->previous].next = entry->next;
    else heads_[entry->group][type] = entry->next;
    if (entry->next != none) entries_[entry->next].previous = entry->previous;
    entry->token &= 0x1ffff;
    entry->next = free_;
    free_ = index;
    lock.unlock();
    return callback(payload);
}

uint32_t AvsDescriptors::setGroup(uint32_t group) {
    if (group >= 4) throw std::invalid_argument("AVS descriptor group must be below four");
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const auto previous = group_;
    group_ = group;
    return previous;
}

void AvsDescriptors::finishCleanup(uint32_t head, uint32_t tail,
    std::unique_lock<std::recursive_mutex> &lock) {
    lock.unlock();
    for (auto index = head; index != none; index = entries_[index].next) {
        const auto callback = entries_[index].close;
        if (!callback) throw std::logic_error("AVS cleanup encountered an unbound owner callback");
        callback(entries_[index].payload);
    }
    lock.lock();
    if (head != none) {
        entries_[tail].next = free_;
        free_ = head;
    }
}

void AvsDescriptors::cleanupType(uint32_t type) {
    if (type == 0 || type >= 64)
        throw std::invalid_argument("AVS descriptor cleanup type must be 1 through 63");
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    const auto head = heads_[group_][type];
    heads_[group_][type] = none;
    auto tail = none;
    for (auto index = head; index != none; index = entries_[index].next) {
        entries_[index].deferred = true;
        tail = index;
    }
    // Original cleanup preserves tokens; lookups still match until reuse,
    // while individual release sees the deferred flag and returns zero.
    finishCleanup(head, tail, lock);
}

void AvsDescriptors::cleanupGroup(uint32_t group) {
    if (group >= 4) throw std::invalid_argument("AVS descriptor group must be below four");
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    auto tail = none;
    auto callbackHead = none;
    for (uint32_t type = 63; type != 0; --type) {
        const auto head = heads_[group][type];
        heads_[group][type] = none;
        if (callbackHead == none) callbackHead = head;
        if (tail != none) entries_[tail].next = head;
        for (auto index = head; index != none; index = entries_[index].next) {
            entries_[index].deferred = true;
            tail = index;
        }
    }
    finishCleanup(callbackHead, tail, lock);
}
} // namespace rb
