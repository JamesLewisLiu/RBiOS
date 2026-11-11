#pragma once
#include "Property.hpp"
#include <memory>
#include <utility>
namespace rb {
// XML syntax/encoding is an explicit platform boundary. Names are UTF-8 ASCII
// in the recovered subset; text/attribute values use the document encoding.
class AvsXmlPropertyBuilder {
public:
    AvsXmlPropertyBuilder(uint8_t encoding, bool allowDuplicateNodes);
    ~AvsXmlPropertyBuilder();
    AvsXmlPropertyBuilder(const AvsXmlPropertyBuilder &) = delete;
    AvsXmlPropertyBuilder &operator=(const AvsXmlPropertyBuilder &) = delete;
    void begin(const std::string &name, const std::vector<std::pair<std::string, Bytes>> &attributes);
    void text(const Bytes &bytes);
    void end(const std::string &name);
    PropertyDocument finish();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rb
