#pragma once
#include <array>
#include <cstdint>
namespace rb {
// Original global character fields at 102AEDCC..102AEDE0, not an ABI overlay.
struct OriginalEntryIdentity {
    std::array<uint8_t,21> bytes{{'M','B','R',0,'J',0,'A',0,'A',0,'2','0','1','3','0','9','1','7','0','0',0}};
    // Only the identity mutation portion of dll_entry_init (10168840..10168983).
    // Returns the region predicate subsequently passed to 101686D0, NOT boot success.
    bool apply(const std::array<uint8_t,16> &softwareId);
};
}
