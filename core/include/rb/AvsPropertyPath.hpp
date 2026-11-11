#pragma once
#include "Property.hpp"
namespace rb {
enum class AvsPathError { none, syntax, reservedAttribute, unsupported };
struct AvsPathStep { std::string name; int32_t selector = 0; bool attributeContext = false; };
struct AvsPropertyPath {
    std::vector<AvsPathStep> steps;
    std::string attribute;
    bool hasAttribute = false;
    AvsPathError error = AvsPathError::none;
};
// Bounded plain-name subset of original 0x100575D0. Leading slashes do
// not change context. Attribute syntax is attribute@node/path.
AvsPropertyPath parseAvsPropertyPath(const std::string &);
struct AvsPropertyReference {
    const Property *node = nullptr; // Attribute owner when attribute != nullptr.
    const Bytes *attribute = nullptr;
    AvsPathError error = AvsPathError::none;
};
AvsPropertyReference referAvsProperty(const Property &context, const std::string &path);
} // namespace rb
