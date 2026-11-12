#pragma once
#include <cstdint>

namespace rb {
// Shared native counterpart of the original TLS worker-record pointer. Owners
// bind their actual stable object identity; a task descriptor is metadata only.
class AvsThreadLocal {
public:
    static void bind(uintptr_t identity, uint32_t descriptor = 0);
    static void unbind(uintptr_t expectedIdentity);
    static uint32_t setCurrentDescriptor(uintptr_t expectedIdentity, uint32_t descriptor);
    static void setCurrentTask(uintptr_t expectedIdentity, uint32_t descriptor, bool attached);
    static uintptr_t currentIdentity() noexcept;
    static uint32_t currentDescriptor() noexcept;
    static int32_t self() noexcept; // Original export 079 / 1001A9F0.
};
} // namespace rb
