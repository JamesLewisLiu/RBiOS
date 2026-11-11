#pragma once
#include "AvsBootConfig.hpp"
#include "Property.hpp"

namespace rb {
// The 48-entry boot psmap at libavs-win32.dll 0x10076C20. Offsets refer
// to the original 2528-byte image, never to host C++ object layout.
struct AvsConfigField {
    uint8_t handleType;
    uint16_t offset;
    uint16_t size;
    const char *path;
    uint64_t defaultInteger;
    const char *defaultString;
};
const std::array<AvsConfigField, 48> &avsBootConfigFields();

// 0x10013A70 / 0x10006C8C: zero image, import in table order, use defaults
// for missing nodes. Input is an already-decoded property tree, not XML.
// Base node types must match. Array flags are ignored by original node_read;
// scalar byte counts must still match. Oversized values throw Error.
// This is configuration import only, not AVS boot or host service startup.
AvsBootConfigImage importAvsBootConfig(const Property *config = nullptr);
} // namespace rb
