#pragma once
#include "rb/AvsDefaultFile.hpp"
#include "rb/AvsMount.hpp"
#include <memory>
namespace rb {
class AvsMountedFile;
class AvsMountRegistry {
public:
    struct State;
    explicit AvsMountRegistry(uint32_t descriptorCapacity = 256, uint32_t mountCapacity = 32);
    // Explicitly registers the recovered read-only native capability. This is
    // not the original addfs API, which requires all 27 filesystem callbacks.
    void registerReadOnlyDefaultFilesystem();
    int32_t mount(const AvsMountRequest &);
    int32_t unmount(std::string_view virtualPath);
    int32_t open(std::string_view virtualPath, std::unique_ptr<AvsMountedFile> &,
                 uint32_t flags = 1, uint16_t permissions = 0444);
    int32_t convertPath(std::string_view virtualPath, std::string &hostPath) const;
    size_t mountedCount() const;
private:
    std::shared_ptr<State> state_;
};
class AvsMountedFile {
public:
    ~AvsMountedFile();
    AvsMountedFile(const AvsMountedFile &) = delete;
    AvsMountedFile &operator=(const AvsMountedFile &) = delete;
    int32_t read(void *buffer, uint32_t size) { return file_.read(buffer, size); }
    int32_t readv(const std::vector<AvsReadBuffer> &buffers) { return file_.readv(buffers); }
    int64_t seek(int64_t offset, int origin) { return file_.seek(offset, origin); }
    bool stat(AvsFileStat &info) const { return file_.stat(info); }
    int32_t close();
private:
    friend class AvsMountRegistry;
    AvsMountedFile(std::shared_ptr<AvsMountRegistry::State>, uint32_t mount);
    std::shared_ptr<AvsMountRegistry::State> state_;
    uint32_t mount_;
    AvsDefaultFile file_;
};
}
