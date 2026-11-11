#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace rb {
struct AvsResolvedPath {
    int32_t status = 0; // negative AVS error, or (component count << 16) | length
    std::string path;
    std::vector<uint32_t> prefixHashes; // root hash followed by component hashes
};
// Recovered 0x1002C110. The original mount wrapper uses capacity 128 and the
// general resolver uses 256. Capacity includes the final NUL. No host I/O.
// Embedded NULs retain original C-string termination behavior.
AvsResolvedPath resolveAvsPath(std::string_view currentDirectory, std::string_view input,
                               uint32_t capacity = 256);
}
