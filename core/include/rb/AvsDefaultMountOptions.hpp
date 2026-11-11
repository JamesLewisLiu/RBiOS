#pragma once
#include <cstdint>
#include <string_view>
namespace rb {
struct AvsDefaultMountOptions { bool posix = false, volumeFlush = true; };
// Original default-fs mount option parser and callbacks. Returns zero or
// 0x80070016; output preserves earlier callback effects on a later failure.
int32_t parseAvsDefaultMountOptions(std::string_view, AvsDefaultMountOptions &);
}
