#include "rb/AvsPropertyPath.hpp"
#include <cerrno>
#include <cstdlib>
#include <limits>
namespace rb {
namespace {
bool supportedName(const std::string &name) {
    if (name.empty() || name.size() > 4096) return false;
    for (unsigned char c : name)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == ':')) return false;
    return true;
}
}
AvsPropertyPath parseAvsPropertyPath(const std::string &path) {
    AvsPropertyPath result;
    if (path.find('\0') != std::string::npos) { result.error = AvsPathError::unsupported; return result; }
    size_t pos = 0;
    while (pos < path.size()) {
        while (pos < path.size() && path[pos] == '/') ++pos;
        if (pos == path.size()) break;
        const size_t start = pos++;
        while (pos < path.size() && path[pos] != '/' && path[pos] != '@' && path[pos] != '#') ++pos;
        const std::string name = path.substr(start, pos - start);
        if (pos < path.size() && path[pos] == '@') {
            if (result.hasAttribute) { result.error = AvsPathError::syntax; return result; }
            result.hasAttribute = true; result.attribute = name; ++pos; continue;
        }
        int32_t selector = 0;
        if (pos < path.size() && path[pos] == '#') {
            const char *begin = path.c_str() + ++pos; char *end = nullptr;
            errno = 0;
            const auto value = std::strtoll(begin, &end, 0);
            if (end == begin || (*end != '\0' && *end != '/')) { result.error = AvsPathError::syntax; return result; }
            if (errno == ERANGE || value < INT32_MIN || value > INT32_MAX) { result.error = AvsPathError::unsupported; return result; }
            selector = value < 0 ? int32_t(value) : int32_t(uint32_t(value) + 1u);
            pos = size_t(end - path.c_str());
        }
        if (!supportedName(name)) { result.error = AvsPathError::unsupported; return result; }
        result.steps.push_back({name, selector, result.hasAttribute});
    }
    if (result.hasAttribute) {
        if (!supportedName(result.attribute)) result.error = AvsPathError::unsupported;
        else if (result.attribute.rfind("__", 0) == 0) result.error = AvsPathError::reservedAttribute;
    }
    return result;
}
AvsPropertyReference referAvsProperty(const Property &context, const std::string &path) {
    const auto query = parseAvsPropertyPath(path);
    const Property *node = &context;
    for (const auto &step : query.steps) {
        const Property *found = nullptr;
        int64_t remaining = step.selector;
        if (remaining >= 0) {
            for (const auto &child : node->children)
                if (child.name == step.name && --remaining <= 0) { found = &child; break; }
        } else {
            for (auto it = node->children.rbegin(); it != node->children.rend(); ++it)
                if (it->name == step.name && ++remaining >= 0) { found = &*it; break; }
        }
        if (!found) return {};
        node = found;
    }
    // The original parser invokes each lookup immediately. A missing earlier
    // component short-circuits errors discovered later in the path.
    if (query.error != AvsPathError::none) return {nullptr, nullptr, query.error};
    if (!query.hasAttribute) return {node, nullptr, AvsPathError::none};
    const auto found = node->attributes.find(query.attribute);
    return found == node->attributes.end() ? AvsPropertyReference{} : AvsPropertyReference{node, &found->second, AvsPathError::none};
}
} // namespace rb
