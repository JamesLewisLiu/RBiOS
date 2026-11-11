#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace rb {
// AVS2 2.15.8 r6631 configuration image after property_psmap_import.
// This byte representation preserves original offsets without imposing x86 ABI
// on ARM64 objects. Parsing and the remaining boot services are not implemented here.
using AvsBootConfigImage = std::array<uint8_t, 2528>;
struct AvsProductFix {
    enum class Field { netSciEnable, mutexDelay, debugLocalOffset } field;
    int32_t previous = 0;
};
// 0x10013B30, called by AVS boot only when imported product mode is enabled.
// Returns original log events in their original order, not a boot-success flag.
std::vector<AvsProductFix> applyAvsProductMode(AvsBootConfigImage &, bool productMode);
} // namespace rb
