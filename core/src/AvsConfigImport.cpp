#include "rb/AvsConfigImport.hpp"
#include <algorithm>
#include <cstring>

namespace rb {
namespace {
constexpr std::array<AvsConfigField, 48> fields{{
    {50, 0, 1, "mode/product", 0ull, nullptr},
    {50, 1, 1, "log/netsci/enable", 1ull, nullptr},
    {10, 2, 65, "log/netsci/server", 0, "localhost:42197"},
    {5, 68, 2, "desc/nr_desc", 808ull, nullptr},
    {5, 104, 2, "thread/nr_thread", 32ull, nullptr},
    {5, 106, 2, "thread/nr_mutex", 64ull, nullptr},
    {5, 108, 2, "thread/nr_cond", 32ull, nullptr},
    {5, 110, 2, "thread/nr_event", 32ull, nullptr},
    {5, 112, 2, "thread/nr_semaphore", 16ull, nullptr},
    {5, 114, 2, "thread/nr_cond_state", 32ull, nullptr},
    {5, 116, 2, "thread/nr_alarm", 16ull, nullptr},
    {7, 96, 4, "thread/min_stack", 4096ull, nullptr},
    {7, 100, 4, "thread/max_stack", 2097152ull, nullptr},
    {5, 118, 2, "thread/mutex_delay", 0ull, nullptr},
    {7, 120, 4, "thread/priority", 2ull, nullptr},
    {5, 128, 2, "fs/nr_filesys", 8ull, nullptr},
    {5, 130, 2, "fs/nr_mountpoint", 16ull, nullptr},
    {5, 132, 2, "fs/nr_mounttable", 4ull, nullptr},
    {5, 134, 2, "fs/nr_filedesc", 32ull, nullptr},
    {5, 136, 2, "fs/link_limit", 4ull, nullptr},
    {10, 138, 17, "fs/root/fstype", 0, "fs"},
    {10, 155, 257, "fs/root/device", 0, "."},
    {10, 412, 257, "fs/root/option", 0, "posix=1"},
    {10, 1731, 256, "fs/mounttable", 0, ""},
    {10, 669, 17, "fs/nvram/fstype", 0, "nvram"},
    {10, 686, 257, "fs/nvram/device", 0, ""},
    {10, 943, 257, "fs/nvram/option", 0, "sz_block=512,nr_block=32,nr_mirror=4"},
    {10, 1200, 17, "fs/raw/fstype", 0, "fs"},
    {10, 1217, 257, "fs/raw/device", 0, ""},
    {10, 1474, 257, "fs/raw/option", 0, "posix=1"},
    {5, 1988, 2, "net/nr_protocol", 16ull, nullptr},
    {5, 1990, 2, "net/nr_socket", 32ull, nullptr},
    {50, 1997, 1, "net/enable_raw", 0ull, nullptr},
    {50, 1998, 1, "net/lz77sock/enable", 0ull, nullptr},
    {50, 1999, 1, "net/multisock/enable", 0ull, nullptr},
    {50, 2000, 1, "net/eaudp/enable", 1ull, nullptr},
    {10, 2002, 256, "net/config_file", 0, ""},
    {5, 1994, 2, "net/addrinfo_nr_cache", 8ull, nullptr},
    {5, 1992, 2, "net/addrinfo_expire", 5400ull, nullptr},
    {50, 1996, 1, "net/addrinfo_log", 0ull, nullptr},
    {6, 2260, 4, "time/gmt_offset", 2147483647ull, nullptr},
    {6, 2264, 4, "time/local_offset", 0ull, nullptr},
    {10, 2272, 256, "time/config_file", 0, "/dev/nvram/clock.xml"},
    {6, 2268, 4, "time/debug_local_offset", 0ull, nullptr},
    {9, 72, 8, "thread/pool/affinity_mask", 18446744073709551615ull, nullptr},
    {7, 80, 4, "thread/pool/stack_size", 65536ull, nullptr},
    {7, 84, 4, "thread/pool/expire_count", 100ull, nullptr},
    {5, 88, 2, "thread/pool/init_thread", 8ull, nullptr},
}};
const Property *find(const Property *node, const std::string &path) {
    size_t begin = 0;
    while (node) {
        const auto end = path.find('/', begin);
        node = node->child(path.substr(begin, end == std::string::npos ? end : end - begin));
        if (end == std::string::npos) return node;
        begin = end + 1;
    }
    return nullptr;
}
} // namespace
const std::array<AvsConfigField, 48> &avsBootConfigFields() { return fields; }
AvsBootConfigImage importAvsBootConfig(const Property *config) {
    if (config && config->name != "config") throw Error("AVS boot: bad configuration root name");
    AvsBootConfigImage image{};
    for (const auto &field : fields) {
        const auto *node = find(config, field.path);
        auto destination = image.begin() + field.offset;
        if (!node) {
            if (field.defaultString) {
                const auto size = std::strlen(field.defaultString);
                if (size >= field.size) throw Error("AVS boot: default string exceeds field");
                std::copy_n(field.defaultString, size, destination);
            } else {
                for (size_t i = 0; i < field.size; ++i)
                    destination[i] = uint8_t(field.defaultInteger >> (8 * i));
            }
            continue;
        }
        const uint8_t nodeType = field.handleType == 10 ? 11 : field.handleType == 50 ? 52 : field.handleType;
        if (node->type != nodeType)
            throw Error(std::string("AVS boot: property type mismatch at ") + field.path);
        if (nodeType == 11) {
            // 0x100340CC copies the recorded byte count verbatim. It neither
            // scans for a terminator nor truncates or converts the string.
            if (node->value.size() > field.size)
                throw Error(std::string("AVS boot: string exceeds field at ") + field.path);
            std::copy(node->value.begin(), node->value.end(), destination);
        } else {
            if (node->value.size() != field.size)
                throw Error(std::string("AVS boot: scalar size mismatch at ") + field.path);
            std::reverse_copy(node->value.begin(), node->value.end(), destination);
        }
    }
    return image;
}
} // namespace rb
