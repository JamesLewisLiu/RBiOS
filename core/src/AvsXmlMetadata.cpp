#include "rb/AvsXmlMetadata.hpp"
#include <algorithm>
#include <limits>
namespace rb {
namespace {
struct Type { std::string_view name; uint8_t value; };
constexpr Type types[] = {
    {"2b", 53},
    {"2d", 25},
    {"2f", 24},
    {"2s16", 18},
    {"2s32", 20},
    {"2s64", 22},
    {"2s8", 16},
    {"2u16", 19},
    {"2u32", 21},
    {"2u64", 23},
    {"2u8", 17},
    {"3b", 54},
    {"3d", 35},
    {"3f", 34},
    {"3s16", 28},
    {"3s32", 30},
    {"3s64", 32},
    {"3s8", 26},
    {"3u16", 29},
    {"3u32", 31},
    {"3u64", 33},
    {"3u8", 27},
    {"4b", 55},
    {"4d", 45},
    {"4f", 44},
    {"4s16", 38},
    {"4s32", 40},
    {"4s64", 42},
    {"4s8", 36},
    {"4u16", 39},
    {"4u32", 41},
    {"4u64", 43},
    {"4u8", 37},
    {"b", 52},
    {"bin", 10},
    {"binary", 10},
    {"bool", 52},
    {"d", 15},
    {"double", 15},
    {"f", 14},
    {"float", 14},
    {"ip4", 12},
    {"s16", 4},
    {"s32", 6},
    {"s64", 8},
    {"s8", 2},
    {"str", 11},
    {"string", 11},
    {"time", 13},
    {"u16", 5},
    {"u32", 7},
    {"u64", 9},
    {"u8", 3},
    {"vb", 56},
    {"vd", 25},
    {"vf", 44},
    {"void", 1},
    {"vs16", 50},
    {"vs32", 40},
    {"vs64", 22},
    {"vs8", 48},
    {"vu16", 51},
    {"vu32", 41},
    {"vu64", 23},
    {"vu8", 49},
};
bool space(unsigned char c) { return c == ' ' || (c >= 9 && c <= 13); }
}
uint8_t avsXmlType(std::string_view text) {
    text = text.substr(0, text.find('\0'));
    const auto found = std::lower_bound(std::begin(types), std::end(types), text,
        [](const Type &type, std::string_view name) { return type.name < name; });
    return found != std::end(types) && found->name == text ? found->value : 0;
}
AvsXmlCount avsXmlCount(std::string_view text) {
    text = text.substr(0, text.find('\0'));
    size_t pos = 0;
    while (pos < text.size() && space(uint8_t(text[pos]))) ++pos;
    if (pos == text.size()) return {};
    const bool negative = text[pos] == '-';
    if (negative && ++pos == text.size()) return {};
    unsigned base = 10;
    if (text[pos] == '0') {
        if (++pos == text.size()) return {1, 0};
        base = 8;
        if (text[pos] == 'x' || text[pos] == 'X') { base = 16; if (++pos == text.size()) return {}; }
    }
    uint64_t value = 0;
    for (; pos < text.size(); ++pos) {
        const unsigned char c = uint8_t(text[pos]);
        unsigned digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else { if (space(c)) break; return {}; }
        // Original 0x10024AF0 accepts digit == radix (e.g. octal '8').
        if (digit > base || value > std::numeric_limits<uint64_t>::max() / base) return {};
        const uint64_t next = value * base + digit;
        if (next < value) return {};
        value = next;
    }
    if ((negative && value != 0) || value > UINT32_MAX) return {};
    return {0, uint32_t(value)};
}
} // namespace rb
