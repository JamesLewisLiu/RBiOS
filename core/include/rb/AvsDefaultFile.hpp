#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace rb {
struct AvsReadBuffer { void *data; uint32_t size; };
struct AvsFileStat {
    int64_t createdMilliseconds = 0, modifiedMilliseconds = 0, accessedMilliseconds = 0;
    uint32_t links = 0;
    uint64_t size = 0;
    uint16_t type = 0, permissions = 0; // original: directory=1, file=2, permissions=0644
};
// Real host implementation of the read-only default-fs callbacks. The caller
// must supply the resolved device path; this is not the AVS mount registry.
class AvsDefaultFile {
public:
    AvsDefaultFile() = default;
    ~AvsDefaultFile();
    AvsDefaultFile(const AvsDefaultFile &) = delete;
    AvsDefaultFile &operator=(const AvsDefaultFile &) = delete;
    int32_t open(const std::string &hostPath, uint32_t flags = 1,
                 uint16_t permissions = 0444, bool posixNames = true);
    int32_t read(void *buffer, uint32_t size);
    int32_t readv(const std::vector<AvsReadBuffer> &);
    int64_t seek(int64_t offset, int origin);
    int32_t close();
    bool stat(AvsFileStat &) const;
    bool isOpen() const { return handle_ != -1; }
private:
    intptr_t handle_ = -1;
};
}
