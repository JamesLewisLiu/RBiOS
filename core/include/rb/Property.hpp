#pragma once
#include "Binary.hpp"
#include <map>

namespace rb {
// Data bytes retain original big-endian representation and original string encoding.
struct Property {
    std::string name;
    uint8_t type = 1;
    bool array = false;
    Bytes value;
    std::map<std::string, Bytes> attributes;
    std::vector<Property> children;
    const Property *child(const std::string &n) const;
    std::vector<int64_t> integers() const;
    std::string string() const;
};
struct PropertyDocument {
    uint8_t encoding = 0x80;
    Property root;
    static PropertyDocument decode(const Bytes &);
    Bytes encode() const;
};
} // namespace rb
