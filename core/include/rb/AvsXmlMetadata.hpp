#pragma once
#include <cstdint>
#include <string_view>
namespace rb {
// Original __type lookup: 65 aliases, case sensitive, 0 for no match.
uint8_t avsXmlType(std::string_view);
struct AvsXmlCount {
    int status = -1; // Original helper: -1 error, 0 success, 1 direct zero.
    uint32_t value = 0;
};
// 0x10024AF0 with the exact unsigned [0,UINT32_MAX] arguments used by
// XML __size / __count at 0x10035740. Not a general integer parser.
AvsXmlCount avsXmlCount(std::string_view);
} // namespace rb
