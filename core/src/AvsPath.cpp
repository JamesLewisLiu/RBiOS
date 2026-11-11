#include "rb/AvsPath.hpp"
namespace rb {
namespace {
constexpr int32_t invalid = -2147024874; // 0x80070016
constexpr int32_t tooLong = -2147024860; // 0x80070024
constexpr uint32_t allowed[] = {0x1, 0x07ffe811, 0x87ffffff, 0x47fffffe, 0, 0, 0, 0};
bool valid(unsigned char c) { return (allowed[c >> 5] & (uint32_t(1) << (c & 31))) != 0; }
}
AvsResolvedPath resolveAvsPath(std::string_view cwd, std::string_view input, uint32_t capacity) {
    // Original callers use fixed 128/256-byte buffers. Bound the portable API;
    // capacities below two would let the original root case overwrite a buffer.
    if (capacity < 2 || capacity > 65535) return {tooLong, {}, {}};
    size_t baseIndex = 0, inputIndex = 0;
    bool base = !cwd.empty() && cwd.front() != 0 && (input.empty() || input.front() != '/');
    auto next = [&]() -> unsigned char {
        if (base) {
            auto c = baseIndex < cwd.size() ? static_cast<unsigned char>(cwd[baseIndex++]) : 0;
            if (c != 0) return c;
            base = false; return '/';
        }
        return inputIndex < input.size() ? static_cast<unsigned char>(input[inputIndex++]) : 0;
    };
    std::string out;
    std::vector<uint32_t> hashes;
    std::vector<uint16_t> lengths;
    uint32_t hash = 0;
    auto finish = [&]() {
        if (out.empty()) out = "/";
        const auto status = int32_t((hashes.size() << 16) | out.size());
        hashes.push_back(hash);
        return AvsResolvedPath{status, out, hashes};
    };
    while (true) {
        unsigned char c = next();
        if (!valid(c)) return {invalid, {}, {}};
        if (c == '/') continue;
        if (c == 0) return finish();
        unsigned dots = 0;
        if (c == '.') {
            dots = 1; c = next();
            if (!valid(c)) return {invalid, {}, {}};
            if (c == '/') continue;
            if (c == 0) return finish();
            if (c == '.') {
                dots = 2; c = next();
                if (!valid(c)) return {invalid, {}, {}};
                if (c == '/' || c == 0) {
                    if (!hashes.empty()) {
                        hash = hashes.back(); hashes.pop_back();
                        out.resize(lengths.back()); lengths.pop_back();
                    }
                    if (c == 0) return finish();
                    continue;
                }
            }
        }
        if (capacity <= out.size() + dots + 2 || hashes.size() >= 64) return {tooLong, {}, {}};
        hashes.push_back(hash); lengths.push_back(uint16_t(out.size()));
        out += '/';
        for (unsigned i = 0; i < dots; ++i) { out += '.'; hash = hash * 31 + '.'; }
        out += char(c); hash = hash * 31 + c;
        while (true) {
            c = next();
            if (!valid(c)) return {invalid, {}, {}};
            if (c == '/') break;
            if (c == 0) return finish();
            if (capacity <= out.size() + 1) return {tooLong, {}, {}};
            out += char(c); hash = hash * 31 + c;
        }
    }
}
}
