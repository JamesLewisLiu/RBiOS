#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace rb {
class AvsTaskCompletionRegistry {
public:
    using Handler = void (*)(uint32_t descriptor);
    static constexpr int32_t unavailable = int32_t(0x8001000cu);
    AvsTaskCompletionRegistry();
    int32_t add(Handler);    // 10041960 / export 044 option 5.
    int32_t remove(Handler); // 10041930 / export 044 option 6.
    void dispatch(uint32_t descriptor) const; // 100418C0, live per-slot loads.
    std::array<Handler,4> entries() const noexcept;
private:
    std::array<std::atomic<Handler>,4> handlers_;
    std::mutex mutation_;
};
} // namespace rb
