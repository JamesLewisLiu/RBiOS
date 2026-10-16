#include "rb/Property.hpp"
#include <algorithm>
#include <functional>

namespace rb {
namespace {
constexpr const char *alphabet = "0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
size_t width(uint8_t t) {
    if (t >= 2 && t <= 9)
        return size_t(1) << ((t - 2) / 2);
    if (t == 12 || t == 13 || t == 14)
        return 4;
    if (t == 15)
        return 8;
    if (t >= 16 && t <= 45) {
        size_t i = (t - 16) % 10;
        size_t w = i < 8 ? (size_t(1) << (i / 2)) : (i == 8 ? 4 : 8);
        return w * (2 + (t - 16) / 10);
    }
    if (t >= 48 && t <= 51)
        return 16;
    if (t == 52)
        return 1;
    if (t >= 53 && t <= 55)
        return t - 51;
    if (t == 56)
        return 16;
    throw Error("unsupported property type " + std::to_string(t));
}
size_t align4(size_t n) {
    return (n + 3) & ~size_t(3);
}
void be32(Bytes &b, uint32_t v) {
    for (int i = 3; i >= 0; --i)
        b.push_back(uint8_t(v >> (8 * i)));
}
std::string name(Reader &r, bool packed) {
    size_t len = r.u8();
    if (!packed) {
        len = (len & ~size_t(64)) + 1;
        auto b = r.take(len);
        return {b.begin(), b.end()};
    }
    std::string s;
    uint32_t acc = 0;
    unsigned bits = 0;
    for (size_t i = 0; i < len; ++i) {
        while (bits < 6) {
            acc = (acc << 8) | r.u8();
            bits += 8;
        }
        bits -= 6;
        s += alphabet[(acc >> bits) & 63];
    }
    return s;
}
void putName(Bytes &b, const std::string &s) {
    if (s.empty() || s.size() > 255)
        throw Error("property name length");
    b.push_back(uint8_t(s.size()));
    uint32_t acc = 0;
    unsigned bits = 0;
    for (char c : s) {
        auto p = std::strchr(alphabet, c);
        if (!p || c == 0)
            throw Error("unsupported property name");
        acc = (acc << 6) | uint32_t(p - alphabet);
        bits += 6;
        while (bits >= 8) {
            bits -= 8;
            b.push_back(uint8_t(acc >> bits));
        }
    }
    if (bits)
        b.push_back(uint8_t(acc << (8 - bits)));
}
struct DataRead {
    const Bytes &b;
    size_t p = 0, byte = 0, word = 0;
    Bytes take(size_t n, size_t &pos) {
        if (pos > b.size() || n > b.size() - pos)
            throw Error("property data truncated");
        Bytes v(b.begin() + pos, b.begin() + pos + n);
        pos += n;
        return v;
    }
    Bytes variable() {
        Reader r(b, p);
        auto n = r.be32();
        p = r.position();
        auto v = take(n, p);
        p = align4(p);
        if (p > b.size())
            throw Error("property data alignment");
        return v;
    }
    Bytes scalar(size_t n) {
        if (byte % 4 == 0)
            byte = p;
        if (word % 4 == 0)
            word = p;
        Bytes v;
        if (n == 1)
            v = take(n, byte);
        else if (n == 2)
            v = take(n, word);
        else {
            v = take(n, p);
            p = align4(p);
        }
        p = std::max(p, align4(std::max(byte, word)));
        if (p > b.size())
            throw Error("property scalar alignment");
        return v;
    }
};
struct DataWrite {
    Bytes b;
    size_t byte = 0, word = 0;
    void variable(const Bytes &v) {
        if (v.size() > UINT32_MAX)
            throw Error("property too large");
        be32(b, uint32_t(v.size()));
        b.insert(b.end(), v.begin(), v.end());
        b.resize(align4(b.size()));
    }
    void scalar(const Bytes &v) {
        if (byte % 4 == 0)
            byte = b.size();
        if (word % 4 == 0)
            word = b.size();
        if (v.size() == 1 || v.size() == 2) {
            auto &p = v.size() == 1 ? byte : word;
            if (p % 4 == 0)
                b.resize(b.size() + 4);
            if (p + v.size() > b.size())
                throw Error("property packing");
            std::copy(v.begin(), v.end(), b.begin() + p);
            p += v.size();
        } else {
            b.insert(b.end(), v.begin(), v.end());
            b.resize(align4(b.size()));
        }
    }
};
} // namespace
const Property *Property::child(const std::string &n) const {
    for (const auto &c : children)
        if (c.name == n)
            return &c;
    return nullptr;
}
std::string Property::string() const {
    auto end = value.size();
    while (end && value[end - 1] == 0)
        --end;
    return {value.begin(), value.begin() + end};
}
std::vector<int64_t> Property::integers() const {
    size_t w = 0;
    bool sign = false;
    if (type >= 2 && type <= 9) {
        w = size_t(1) << ((type - 2) / 2);
        sign = !(type & 1);
    } else if (type >= 16 && type <= 45 && ((type - 16) % 10) < 8) {
        auto t = (type - 16) % 10;
        w = size_t(1) << (t / 2);
        sign = !(t & 1);
    } else if (type == 12 || type == 13) {
        w = 4;
    } else if (type >= 48 && type <= 51) {
        w = type < 50 ? 1 : 2;
        sign = !(type & 1);
    } else if (type >= 52 && type <= 56) {
        w = 1;
    } else
        throw Error("property is not integer");
    if (value.size() % w)
        throw Error("integer property size");
    std::vector<int64_t> out;
    for (size_t p = 0; p < value.size(); p += w) {
        uint64_t v = 0;
        for (size_t i = 0; i < w; ++i)
            v = (v << 8) | value[p + i];
        if (sign && w < 8 && (v & (uint64_t(1) << (w * 8 - 1))))
            v |= (~uint64_t(0)) << (w * 8);
        int64_t s;
        std::memcpy(&s, &v, 8);
        out.push_back(s);
    }
    return out;
}
PropertyDocument PropertyDocument::decode(const Bytes &bytes) {
    Reader r(bytes);
    if (r.u8() != 0xa0)
        throw Error("not KBinXML");
    auto mode = r.u8();
    if (mode != 0x42 && mode != 0x45)
        throw Error("KBinXML name encoding");
    PropertyDocument doc;
    doc.encoding = r.u8();
    if ((r.u8() ^ doc.encoding) != 255)
        throw Error("KBinXML encoding check");
    auto n = r.be32();
    auto names = r.take(n);
    auto size = r.be32();
    auto values = r.take(size);
    auto padding = r.take(r.remaining());
    if (padding.size() > 15 ||
        std::any_of(padding.begin(), padding.end(), [](uint8_t v) { return v != 0; }))
        throw Error("KBinXML trailing data");
    Reader nodes(names);
    DataRead data{values};
    Property wrapper;
    std::vector<Property *> stack{&wrapper};
    size_t count = 0;
    bool ended = false;
    while (nodes.remaining()) {
        auto raw = nodes.u8();
        if (!raw)
            continue;
        auto type = uint8_t(raw & ~64);
        bool array = (raw & 64) != 0;
        if (type == 191) {
            ended = true;
            break;
        }
        if (type == 190) {
            if (stack.size() <= 1)
                throw Error("unbalanced property end");
            stack.pop_back();
            continue;
        }
        auto nm = name(nodes, mode == 0x42);
        if (type == 46) {
            if (stack.size() <= 1)
                throw Error("orphan property attribute");
            stack.back()->attributes[nm] = data.variable();
            continue;
        }
        if (stack.size() > 128 || ++count > 1000000)
            throw Error("property tree limit");
        Property p;
        p.name = std::move(nm);
        p.type = type;
        p.array = array;
        if (type != 1) {
            if (type == 10 || type == 11 || array)
                p.value = data.variable();
            else
                p.value = data.scalar(width(type));
            if (type != 10 && type != 11 && p.value.size() % width(type))
                throw Error("property array size");
        }
        stack.back()->children.push_back(std::move(p));
        stack.push_back(&stack.back()->children.back());
    }
    if (!ended || stack.size() != 1 || wrapper.children.size() != 1)
        throw Error("incomplete property tree");
    doc.root = std::move(wrapper.children.front());
    return doc;
}
Bytes PropertyDocument::encode() const {
    Bytes nodes;
    DataWrite data;
    size_t count = 0;
    std::function<void(const Property &, size_t)> emit = [&](const Property &p, size_t depth) {
        if (depth > 128 || ++count > 1000000)
            throw Error("property tree limit");
        nodes.push_back(p.type | (p.array ? 64 : 0));
        putName(nodes, p.name);
        if (p.type != 1) {
            if (p.type == 10 || p.type == 11 || p.array)
                data.variable(p.value);
            else {
                if (p.value.size() != width(p.type))
                    throw Error("property scalar size");
                data.scalar(p.value);
            }
        }
        for (const auto &a : p.attributes) {
            nodes.push_back(46);
            putName(nodes, a.first);
            data.variable(a.second);
        }
        for (const auto &c : p.children)
            emit(c, depth + 1);
        nodes.push_back(0xfe);
    };
    emit(root, 0);
    nodes.push_back(0xff);
    nodes.resize(align4(nodes.size()));
    Bytes b{0xa0, 0x42, encoding, uint8_t(255 ^ encoding)};
    be32(b, uint32_t(nodes.size()));
    b.insert(b.end(), nodes.begin(), nodes.end());
    be32(b, uint32_t(data.b.size()));
    b.insert(b.end(), data.b.begin(), data.b.end());
    return b;
}
} // namespace rb
