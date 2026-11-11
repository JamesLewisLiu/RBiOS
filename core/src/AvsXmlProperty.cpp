#include "rb/AvsXmlProperty.hpp"
#include "rb/AvsXmlMetadata.hpp"
#include <algorithm>
#include <optional>
#include <set>
namespace rb {
namespace {
bool space(uint8_t c) { return c == 32 || (c >= 9 && c <= 13); }
bool blank(const Bytes &b) { return std::all_of(b.begin(), b.end(), space); }
std::string string(const Bytes &b) { return {b.begin(), b.end()}; }
void validateName(const std::string &name) {
    if (name.empty() || name.size() > 4096) throw Error("AVS XML: unsupported name length");
    for (unsigned char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == ':'))
            throw Error("AVS XML: unsupported name alphabet");
}
}
struct AvsXmlPropertyBuilder::Impl {
    struct Frame { Property node; Bytes content; uint8_t requestedType = 0; std::optional<uint32_t> count; };
    PropertyDocument document;
    std::vector<Frame> stack;
    bool duplicates, completed = false, consumed = false;
    Impl(uint8_t encoding, bool allow) : duplicates(allow) { document.encoding = encoding; }
};
AvsXmlPropertyBuilder::AvsXmlPropertyBuilder(uint8_t encoding, bool duplicates) : impl_(new Impl(encoding, duplicates)) {}
AvsXmlPropertyBuilder::~AvsXmlPropertyBuilder() = default;
void AvsXmlPropertyBuilder::begin(const std::string &name, const std::vector<std::pair<std::string, Bytes>> &attributes) {
    auto &state = *impl_;
    if (state.completed || state.consumed) throw Error("AVS XML: multiple root elements");
    validateName(name);
    if (!state.stack.empty()) {
        const auto &parent = state.stack.back();
        if (!blank(parent.content)) throw Error("AVS XML: mixed content is not recovered");
        if (!state.duplicates && parent.node.child(name)) throw Error("AVS XML: duplicate node");
    }
    Impl::Frame frame; frame.node.name = name;
    std::set<std::string> seen;
    for (const auto &[key, bytes] : attributes) {
        validateName(key);
        if (!seen.insert(key).second) throw Error("AVS XML: duplicate attribute");
        if (std::find(bytes.begin(), bytes.end(), 0) != bytes.end()) throw Error("AVS XML: NUL attribute");
        if (key == "__type") {
            frame.requestedType = avsXmlType(string(bytes));
            if (!frame.requestedType) throw Error("AVS XML: unknown type");
        } else if (key == "__count") {
            const auto count = avsXmlCount(string(bytes));
            if (count.status < 0) throw Error("AVS XML: invalid count");
            frame.count = count.value;
        } else if (key == "__size") {
            throw Error("AVS XML: explicit size allocation is not recovered");
        } else if (key.rfind("__", 0) == 0) throw Error("AVS XML: unknown reserved attribute");
        else { frame.node.attributes[key] = bytes; frame.node.attributes[key].push_back(0); }
    }
    const auto t = frame.requestedType;
    if (t != 0 && t != 1 && t != 3 && t != 5 && t != 7 && t != 11 && t != 52)
        throw Error("AVS XML: content type is not recovered");
    if (t == 11 && frame.count) throw Error("AVS XML: strings cannot have count");
    state.stack.push_back(std::move(frame));
}
void AvsXmlPropertyBuilder::text(const Bytes &bytes) {
    auto &state = *impl_;
    if (std::find(bytes.begin(), bytes.end(), 0) != bytes.end()) throw Error("AVS XML: NUL content");
    if (state.stack.empty()) { if (!blank(bytes)) throw Error("AVS XML: text outside root"); return; }
    auto &frame = state.stack.back();
    if (!frame.node.children.empty()) { if (!blank(bytes)) throw Error("AVS XML: mixed content is not recovered"); return; }
    if (frame.content.size() + bytes.size() > 16 * 1024 * 1024) throw Error("AVS XML: content resource limit");
    frame.content.insert(frame.content.end(), bytes.begin(), bytes.end());
}
void AvsXmlPropertyBuilder::end(const std::string &name) {
    auto &state = *impl_;
    if (state.stack.empty() || state.stack.back().node.name != name) throw Error("AVS XML: mismatched end element");
    auto frame = std::move(state.stack.back()); state.stack.pop_back();
    auto &node = frame.node;
    node.type = frame.requestedType;
    if (node.type == 0) node.type = blank(frame.content) ? 1 : 11;
    if (node.type == 11) { node.value = std::move(frame.content); node.value.push_back(0); }
    else if (node.type == 1) {
        if (!blank(frame.content)) throw Error("AVS XML: void content is not recovered");
    } else {
        const size_t width = node.type == 5 ? 2 : node.type == 7 ? 4 : 1;
        const uint32_t maximum = node.type == 52 ? 1 : width == 1 ? 255 : width == 2 ? 65535 : UINT32_MAX;
        const size_t count = frame.count.value_or(1);
        if (count > (16 * 1024 * 1024) / width) throw Error("AVS XML: array resource limit");
        node.array = frame.count.has_value(); node.value.assign(count * width, 0);
        size_t pos = 0, index = 0;
        while (pos < frame.content.size()) {
            while (pos < frame.content.size() && space(frame.content[pos])) ++pos;
            if (pos == frame.content.size()) break;
            const size_t begin = pos;
            while (pos < frame.content.size() && !space(frame.content[pos])) ++pos;
            if (index >= count) throw Error("AVS XML: excess values");
            const auto parsed = avsXmlCount(std::string(frame.content.begin() + begin, frame.content.begin() + pos));
            if (parsed.status < 0 || parsed.value > maximum) throw Error("AVS XML: invalid scalar");
            for (size_t i = 0; i < width; ++i) node.value[index * width + i] = uint8_t(parsed.value >> (8 * (width - i - 1)));
            ++index;
        }
    }
    if (state.stack.empty()) { state.document.root = std::move(node); state.completed = true; }
    else state.stack.back().node.children.push_back(std::move(node));
}
PropertyDocument AvsXmlPropertyBuilder::finish() {
    auto &state = *impl_;
    if (!state.completed || !state.stack.empty() || state.consumed) throw Error("AVS XML: incomplete or consumed document");
    state.consumed = true; return std::move(state.document);
}
} // namespace rb
