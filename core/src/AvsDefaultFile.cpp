#include "rb/AvsDefaultFile.hpp"
#include <cerrno>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
namespace rb {
namespace {
constexpr int32_t avsError(uint32_t code) { return int32_t(0x80070000u | code); }
constexpr int32_t unsupported = avsError(95);
#ifdef _WIN32
#include "AvsFileWindowsErrors.inc"
// Original non-socket branch of 0x100247D0, including unknown-error fallback.
int32_t hostError() {
    const auto error = GetLastError();
    if (!error) return 0;
    for (const auto &entry : windowsErrors) if (entry[0] == error) return avsError(entry[1]);
    return avsError(4095);
}
#else
int32_t hostError() {
    switch (errno) {
    case 0: return 0;
    case ENOENT: return avsError(2);
    case EIO: return avsError(5);
    case EBADF: return avsError(9);
    case ENOMEM: return avsError(12);
    case EACCES: case EPERM: return avsError(13);
    case EINVAL: return avsError(22);
    case EISDIR: return avsError(21);
    case ENOTDIR: return avsError(20);
    case ESPIPE: return avsError(29);
    case ENAMETOOLONG: return avsError(36);
    default: return avsError(4095);
    }
}
// Verify actual directory entry spelling on case-insensitive Apple volumes.
// This is platform glue for original GetLongPathNameA + strcmp semantics.
bool exactCase(const std::string &path) {
    std::string directory = path.front() == '/' ? "/" : ".";
    size_t start = path.front() == '/' ? 1 : 0;
    while (start < path.size()) {
        const size_t end = path.find('/', start);
        const auto name = path.substr(start, end == std::string::npos ? end : end-start);
        if (!name.empty() && name != "." && name != "..") {
            DIR *dir = opendir(directory.c_str());
            if (!dir) return true; // Let open report the native failure.
            bool found = false;
            while (auto *entry = readdir(dir)) if (name == entry->d_name) { found = true; break; }
            closedir(dir);
            if (!found) {
                struct ::stat info{};
                if (::stat((directory+"/"+name).c_str(), &info) == 0) return false;
                return true; // Missing path, not a case mismatch.
            }
        }
        directory += "/" + name;
        if (end == std::string::npos) break;
        start = end+1;
    }
    return true;
}
#endif
}
AvsDefaultFile::~AvsDefaultFile() { if (isOpen()) close(); }
int32_t AvsDefaultFile::open(const std::string &path, uint32_t flags, uint16_t permissions, bool posixNames) {
    if (isOpen()) return avsError(16);
    if ((flags & 3) == 0 || path.empty() || path.find('\0') != std::string::npos) return avsError(22);
    if ((flags & 3) != 1) return unsupported; // Write callbacks are not recovered here.
    // 0x1002A635 selects OPEN_EXISTING immediately for read access, before
    // create/truncate/exclusive processing. Preserve the original ignored
    // bits: only 0x40 is examined again by the read-only open/close path.
#ifdef _WIN32
    if (posixNames) {
        char actual[128]{};
        const auto length = GetLongPathNameA(path.c_str(), actual, sizeof(actual));
        const auto error = GetLastError();
        if (length >= sizeof(actual)) return avsError(36);
        if (length != 0 && path != actual) return avsError(124);
        if (!length && error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) return avsError(124);
    }
    DWORD share = FILE_SHARE_DELETE;
    if (permissions & 0444) share |= FILE_SHARE_READ;
    if (permissions & 0222) share |= FILE_SHARE_WRITE;
    DWORD attributes = (permissions & 0222) ? 0x80000080u : FILE_ATTRIBUTE_READONLY;
    attributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    if (posixNames) attributes |= FILE_FLAG_POSIX_SEMANTICS;
    auto h = CreateFileA(path.c_str(), GENERIC_READ, share, nullptr, OPEN_EXISTING, attributes, nullptr);
    if (h == INVALID_HANDLE_VALUE) return hostError();
    handle_ = reinterpret_cast<intptr_t>(h);
    // The original open callback ignores the initial append seek's result.
    if (flags & 0x40) SetFilePointer(h, 0, nullptr, FILE_END);
#else
    (void)permissions; // POSIX has no equivalent Windows share-mode contract.
    if (posixNames && !exactCase(path)) return avsError(124);
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return hostError();
    handle_ = fd;
    if (flags & 0x40) (void)lseek(fd, 0, SEEK_END);
#endif
    return 0;
}
int32_t AvsDefaultFile::read(void *buffer, uint32_t size) {
    if (!isOpen()) return avsError(9);
    if ((!buffer && size) || size > uint32_t(INT32_MAX)) return avsError(22);
#ifdef _WIN32
    DWORD count = 0;
    if (!ReadFile(reinterpret_cast<HANDLE>(handle_), buffer, size, &count, nullptr)) return hostError();
    return int32_t(count);
#else
    const auto count = ::read(int(handle_), buffer, size);
    return count < 0 ? hostError() : int32_t(count);
#endif
}
int32_t AvsDefaultFile::readv(const std::vector<AvsReadBuffer> &buffers) {
    uint32_t total = 0;
    for (const auto &buffer : buffers) {
        const auto count = read(buffer.data, buffer.size);
        if (count < 0) return count; // Original error discards the prior total.
        total += uint32_t(count);
        if (uint32_t(count) < buffer.size) break;
    }
    return int32_t(total);
}
int64_t AvsDefaultFile::seek(int64_t offset, int origin) {
    if (origin < 0 || origin > 2) return avsError(22);
    if (!isOpen()) return avsError(9);
#ifdef _WIN32
    LONG high = LONG(uint64_t(offset) >> 32);
    SetLastError(0);
    const DWORD low = SetFilePointer(reinterpret_cast<HANDLE>(handle_), LONG(offset), &high, DWORD(origin));
    if (low == INVALID_SET_FILE_POINTER && GetLastError()) return hostError();
    return int64_t((uint64_t(uint32_t(high)) << 32) | low);
#else
    const auto result = lseek(int(handle_), off_t(offset), origin);
    return result < 0 ? hostError() : int64_t(result);
#endif
}
int32_t AvsDefaultFile::close() {
    if (!isOpen()) return avsError(9);
    const auto h = handle_; handle_ = -1;
#ifdef _WIN32
    return CloseHandle(reinterpret_cast<HANDLE>(h)) ? 0 : hostError();
#else
    return ::close(int(h)) == 0 ? 0 : hostError();
#endif
}
bool AvsDefaultFile::stat(AvsFileStat &output) const {
    if (!isOpen()) return false;
    AvsFileStat result;
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(reinterpret_cast<HANDLE>(handle_), &info)) return false;
    auto time = [](FILETIME t) { return int64_t(((uint64_t(t.dwHighDateTime)<<32)|t.dwLowDateTime)/10000)-11644473600000LL; };
    result.createdMilliseconds = time(info.ftCreationTime);
    result.modifiedMilliseconds = time(info.ftLastWriteTime);
    result.accessedMilliseconds = time(info.ftLastAccessTime);
    result.size = (uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    result.links = info.nNumberOfLinks;
    result.type = info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? 1 : 2;
#elif defined(__APPLE__)
    struct ::stat info{};
    if (fstat(int(handle_), &info) != 0) return false;
    auto time = [](timespec t) { return int64_t(t.tv_sec)*1000+t.tv_nsec/1000000; };
    result.createdMilliseconds = time(info.st_birthtimespec);
    result.modifiedMilliseconds = time(info.st_mtimespec);
    result.accessedMilliseconds = time(info.st_atimespec);
    result.size = uint64_t(info.st_size); result.links = uint32_t(info.st_nlink);
    result.type = S_ISDIR(info.st_mode) ? 1 : 2;
#else
    return false; // This adapter has no recovered creation-time source here.
#endif
    result.permissions = 0644;
    output = result; return true;
}
}
