#include "rb/CabinetConfigStore.hpp"
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace rb {
namespace fs = std::filesystem;
namespace {
bool within(const fs::path &child, const fs::path &parent) {
    auto c = child.begin();
    for (auto p = parent.begin(); p != parent.end(); ++p, ++c)
        if (c == child.end() || *c != *p) return false;
    return true;
}
void writeBytes(const fs::path &path, const Bytes &bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw Error("Cannot create cabinet configuration staging file");
    out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
    out.flush();
    if (!out) throw Error("Cannot write cabinet configuration staging file");
    out.close();
    if (!out) throw Error("Cannot close cabinet configuration staging file");
}
}
CabinetConfigStore::CabinetConfigStore(fs::path root, fs::path backups)
    : root_(fs::canonical(root)), backups_(fs::weakly_canonical(fs::absolute(backups))) {
    if (!fs::is_directory(root_) || within(backups_, root_) || within(root_, backups_))
        throw Error("Cabinet backup root must be separate from resource root");
}
void CabinetConfigStore::replace(const fs::path &relative, const Bytes &bytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name())
        throw Error("Cabinet configuration path must be relative");
    for (const auto &part : relative)
        if (part == ".." || part == ".") throw Error("Cabinet configuration path traversal");
    const auto first = *relative.begin();
    if ((first != "prop" && first != "dev") || relative.extension() != ".xml")
        throw Error("Only cabinet XML configuration under prop/dev may be replaced");
    const auto target = root_ / relative;
    // Reject symlinks at every existing component, including the final file.
    auto cursor = root_;
    for (const auto &part : relative) {
        cursor /= part;
        if (fs::is_symlink(fs::symlink_status(cursor))) throw Error("Cabinet configuration symlink");
    }
    if (!fs::is_directory(target.parent_path())) throw Error("Cabinet configuration directory is unavailable");
    const auto backup = backups_ / relative;
    fs::create_directories(backup.parent_path());
    if (!within(fs::canonical(backup.parent_path()), backups_) || fs::is_symlink(fs::symlink_status(backup)))
        throw Error("Cabinet backup path escapes backup root");
    if (fs::exists(target)) {
        if (!fs::is_regular_file(target)) throw Error("Cabinet configuration is not a regular file");
        // First version is retained across later writes. Existing backups are
        // never overwritten. Failure to back up prevents replacement.
        if (!fs::exists(backup)) fs::copy_file(target, backup, fs::copy_options::none);
        else if (!fs::is_regular_file(backup)) throw Error("Cabinet backup is not a regular file");
    }
    // An exclusively created staging directory avoids clobbering user files.
    fs::path staging;
    for (unsigned i = 0; i < 1024; ++i) {
        const auto candidate = target.parent_path() / (".rbios-config-" + std::to_string(i));
        if (fs::create_directory(candidate)) { staging = candidate; break; }
    }
    if (staging.empty()) throw Error("Cannot reserve cabinet configuration staging directory");
    const auto temporary = staging / "replacement";
    try {
        writeBytes(temporary, bytes);
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw Error("Cannot replace cabinet configuration file");
#else
        fs::rename(temporary, target);
#endif
        // Replacement has committed. Cleanup failure must not report a failed
        // write after the destination has already changed.
        std::error_code cleanupError;
        fs::remove(staging, cleanupError);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        fs::remove(staging, ignored);
        throw;
    }
}
}
