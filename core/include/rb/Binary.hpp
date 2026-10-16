#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace rb {
using Bytes = std::vector<uint8_t>;
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};
class Reader {
    const Bytes &bytes_;
    size_t pos_ = 0;

  public:
    explicit Reader(const Bytes &b, size_t p = 0) : bytes_(b), pos_(p) {
        require(0);
    }
    size_t position() const {
        return pos_;
    }
    size_t remaining() const {
        return bytes_.size() - pos_;
    }
    void require(size_t n) const {
        if (pos_ > bytes_.size() || n > bytes_.size() - pos_)
            throw Error("truncated binary at " + std::to_string(pos_));
    }
    void skip(size_t n) {
        require(n);
        pos_ += n;
    }
    uint8_t u8() {
        require(1);
        return bytes_[pos_++];
    }
    uint16_t u16() {
        auto a = u8();
        auto b = u8();
        return uint16_t(a | (uint16_t(b) << 8));
    }
    int16_t i16() {
        auto u = u16();
        int16_t s;
        std::memcpy(&s, &u, 2);
        return s;
    }
    uint32_t u32() {
        auto a = u16();
        auto b = u16();
        return uint32_t(a) | (uint32_t(b) << 16);
    }
    int32_t i32() {
        auto u = u32();
        int32_t s;
        std::memcpy(&s, &u, 4);
        return s;
    }
    uint16_t be16() {
        auto a = u8();
        auto b = u8();
        return uint16_t((uint16_t(a) << 8) | b);
    }
    uint32_t be32() {
        auto a = be16();
        auto b = be16();
        return (uint32_t(a) << 16) | b;
    }
    float f32() {
        auto u = u32();
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    Bytes take(size_t n) {
        require(n);
        Bytes out(bytes_.begin() + pos_, bytes_.begin() + pos_ + n);
        pos_ += n;
        return out;
    }
};
inline Bytes readFile(const std::string &path, size_t limit = 128 * 1024 * 1024) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        throw Error("cannot open: " + path);
    auto size = f.tellg();
    if (size < 0 || uint64_t(size) > limit)
        throw Error("file size exceeds limit: " + path);
    Bytes b(static_cast<size_t>(size));
    f.seekg(0);
    if (!b.empty() && !f.read(reinterpret_cast<char *>(b.data()), b.size()))
        throw Error("read failed: " + path);
    return b;
}
inline std::string cstring(const Bytes &b, size_t off, size_t max) {
    if (off > b.size() || max > b.size() - off)
        throw Error("string range");
    size_t n = 0;
    while (n < max && b[off + n])
        ++n;
    if (n == max)
        throw Error("unterminated string");
    return std::string(reinterpret_cast<const char *>(b.data() + off), n);
}
} // namespace rb
