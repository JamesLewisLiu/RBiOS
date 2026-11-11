#pragma once
#include "rb/AvsBootConfig.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace rb {
struct AvsFilesystemType {
    std::string_view name;
    uint32_t originalFactoryAddress;
};
const std::array<AvsFilesystemType, 6> &avsFilesystemTypes();

// String values are passed verbatim: options is not a numeric flags field.
struct AvsMountRequest {
    std::string path, device, filesystem, options;
};

// No production implementation is supplied. A backend must implement the real
// filesystem services before this recovered boot phase can be used by the app.
// Descriptor tokens are host-sized handles, never relocated x86 pointers.
class AvsFilesystemBackend {
public:
    virtual ~AvsFilesystemBackend() = default;
    virtual uintptr_t createFilesystem(const AvsFilesystemType &) = 0;
    virtual int32_t addFilesystem(uintptr_t descriptor) = 0;
    virtual int32_t mount(const AvsMountRequest &) = 0;
    virtual int32_t openMountTable(std::string_view path) = 0;
    virtual int32_t applyMountTable(int32_t handle, std::string_view selection) = 0;
};

enum class AvsFilesystemFailure { emptyRootDevice, registration, mount, mountTable, unterminatedField };
class AvsFilesystemBootError : public std::runtime_error {
public:
    AvsFilesystemBootError(AvsFilesystemFailure stage, std::string subject, int32_t code);
    AvsFilesystemFailure stage;
    std::string subject;
    int32_t code;
};

// 0x10013FA0 and 0x10014010. Null names/paths skip the operation. An empty
// device also skips mounting. Every invoked operation succeeds only above zero.
bool registerAvsFilesystem(const char *name, uintptr_t descriptor, AvsFilesystemBackend &);
bool mountAvsFilesystem(const char *path, const AvsMountRequest &, AvsFilesystemBackend &);

// 0x10013E00. Returns zero without a mount table, otherwise its positive handle.
// This is a single boot phase, not an assertion that AVS or the game has booted.
// Fatal-log branches become exceptions; existing mounts are not rolled back.
int32_t runAvsFilesystemBoot(const AvsBootConfigImage &, AvsFilesystemBackend &);
} // namespace rb
