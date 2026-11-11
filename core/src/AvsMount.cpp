#include "rb/AvsMount.hpp"
#include <algorithm>

namespace rb {
namespace {
const std::array<AvsFilesystemType, 6> types{{
    {"default", 0x10029510}, {"nvram", 0x1001DAE0},
    {"imagefs", 0x10023270}, {"mirrorfs", 0x1002B270},
    {"cryptfs", 0x1003B550}, {"nvram2", 0x1003CA30}
}};
std::string field(const AvsBootConfigImage &config, size_t offset, size_t capacity) {
    auto first = config.begin() + offset;
    auto last = first + capacity;
    auto end = std::find(first, last, uint8_t(0));
    if (end == last) throw AvsFilesystemBootError(AvsFilesystemFailure::unterminatedField,
                                                std::to_string(offset), 0);
    return std::string(first, end);
}
}
const std::array<AvsFilesystemType, 6> &avsFilesystemTypes() { return types; }
AvsFilesystemBootError::AvsFilesystemBootError(AvsFilesystemFailure s, std::string subject_, int32_t c)
    : std::runtime_error("AVS filesystem boot failed: " + subject_), stage(s), subject(std::move(subject_)), code(c) {}

bool registerAvsFilesystem(const char *name, uintptr_t descriptor, AvsFilesystemBackend &backend) {
    return name == nullptr || backend.addFilesystem(descriptor) > 0;
}
bool mountAvsFilesystem(const char *path, const AvsMountRequest &request, AvsFilesystemBackend &backend) {
    if (path == nullptr || request.device.empty()) return true;
    auto forwarded = request;
    forwarded.path = path;
    return backend.mount(forwarded) > 0;
}

int32_t runAvsFilesystemBoot(const AvsBootConfigImage &config, AvsFilesystemBackend &backend) {
    if (config[155] == 0)
        throw AvsFilesystemBootError(AvsFilesystemFailure::emptyRootDevice, "/config/fs/root/device", 0);
    for (const auto &type : types) {
        const auto descriptor = backend.createFilesystem(type);
        const auto result = backend.addFilesystem(descriptor);
        if (result <= 0) throw AvsFilesystemBootError(AvsFilesystemFailure::registration, std::string(type.name), result);
    }
    constexpr const char *paths[] = {"/", "/dev/nvram", "/dev/raw"};
    for (size_t index = 0; index < 3; ++index) {
        const size_t offset = 138 + index * 531;
        // Original helper checks the first device byte before reading any other
        // field. Preserve that skip even if unused strings are not terminated.
        if (config[offset + 17] == 0) continue;
        AvsMountRequest request{paths[index], field(config, offset + 17, 257),
                                field(config, offset, 17), field(config, offset + 274, 257)};
        const auto result = backend.mount(request);
        if (result <= 0) throw AvsFilesystemBootError(AvsFilesystemFailure::mount, request.path, result);
    }
    if (config[1731] == 0) return 0;
    // The original property-map record reserves 256 bytes here, unlike the
    // 257-byte device/option fields above.
    const auto path = field(config, 1731, 256);
    const auto handle = backend.openMountTable(path);
    if (handle <= 0) throw AvsFilesystemBootError(AvsFilesystemFailure::mountTable, path, handle);
    // Original caller ignores the apply result and returns the open handle.
    backend.applyMountTable(handle, "ALL");
    return handle;
}
} // namespace rb
