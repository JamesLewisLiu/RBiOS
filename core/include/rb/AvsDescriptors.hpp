#pragma once
#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace rb {
// Native recovery of the AVS descriptor pool; tokens remain 32-bit, payloads
// and callbacks do not. The original Windows mutex object pool is separate.
class AvsDescriptors {
public:
    using Close = int32_t (*)(uintptr_t);
    explicit AvsDescriptors(uint32_t capacity);
    uint32_t allocate(uint32_t type, Close close, uintptr_t payload);
    bool lookup(uint32_t token, uintptr_t *payload = nullptr) const;
    bool replace(uint32_t token, Close close, uintptr_t payload);
    int32_t release(uint32_t token);
    uint32_t setGroup(uint32_t group);
    void cleanupType(uint32_t type);
    void cleanupGroup(uint32_t group);
    uint32_t capacity() const noexcept { return uint32_t(entries_.size()) - 4; }
private:
    static constexpr uint32_t none = UINT32_MAX;
    struct Entry {
        uintptr_t payload = 0;
        Close close = nullptr;
        uint32_t previous = none, next = none, token = 0;
        uint8_t group = 0;
        bool deferred = false;
    };
    static int32_t noClose(uintptr_t) { return 0; }
    Entry *find(uint32_t token);
    const Entry *find(uint32_t token) const;
    void link(uint32_t index, uint32_t type);
    void finishCleanup(uint32_t head, uint32_t tail, std::unique_lock<std::recursive_mutex> &lock);
    std::vector<Entry> entries_;
    std::array<std::array<uint32_t, 64>, 4> heads_;
    uint32_t free_ = 4, generation_ = 0;
    uint32_t group_ = 0;
    mutable std::recursive_mutex mutex_;
};
} // namespace rb
