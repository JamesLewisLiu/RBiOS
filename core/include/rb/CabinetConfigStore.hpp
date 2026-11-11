#pragma once
#include "Binary.hpp"
#include <filesystem>
#include <mutex>
namespace rb {
// Platform persistence boundary, not an original AVS export. Callers supply
// original serialized bytes and original write triggers. No profile storage.
class CabinetConfigStore {
public:
    CabinetConfigStore(std::filesystem::path resourceRoot, std::filesystem::path backupRoot);
    void replace(const std::filesystem::path &relative, const Bytes &bytes);
private:
    std::filesystem::path root_, backups_;
    std::mutex mutex_;
};
}
