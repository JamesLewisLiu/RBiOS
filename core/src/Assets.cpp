#include "rb/Assets.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

namespace rb {
IfsArchive::IfsArchive(Bytes b) : bytes_(std::move(b)) {
    Reader r(bytes_);
    if (r.be32() != 0x6cad8f89)
        throw Error("not IFS");
    auto flags = r.be16();
    if ((flags ^ r.be16()) != 65535)
        throw Error("IFS complement");
    r.skip(8);
    base_ = r.be32();
    if (flags > 1)
        r.skip(16);
    if (base_ < r.position() || base_ > bytes_.size())
        throw Error("IFS manifest range");
    auto manifest = PropertyDocument::decode(r.take(base_ - r.position()));
    std::function<void(const Property &, std::string)> walk = [&](const Property &p,
                                                                  std::string parent) {
        if (p.name == "_info_" || p.name == "_super_")
            return;
        auto path = parent.empty() ? p.name : parent + "/" + p.name;
        if (p.type == 30 && p.value.size() == 12) {
            auto values = p.integers();
            if (values[0] < 0 || values[1] < 0 ||
                uint64_t(values[0]) + uint64_t(values[1]) > bytes_.size() - base_)
                throw Error("IFS member outside file");
            entries_.push_back({path, base_ + size_t(values[0]), size_t(values[1])});
        }
        for (const auto &c : p.children)
            walk(c, path);
    };
    for (const auto &c : manifest.root.children)
        walk(c, "");
}
Bytes IfsArchive::read(const std::string &name) const {
    for (const auto &e : entries_)
        if (e.path == name)
            return Bytes(bytes_.begin() + e.offset, bytes_.begin() + e.offset + e.size);
    throw Error("IFS member absent: " + name);
}
Bytes decodeAvsLz(const Bytes &b, size_t expected, size_t limit) {
    if (expected > limit)
        throw Error("texture decompression limit");
    Reader r(b);
    Bytes out;
    out.reserve(expected);
    while (r.remaining()) {
        auto flags = r.u8();
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (flags & (1u << bit)) {
                if (out.size() >= expected)
                    throw Error("AVSLZ output overflow");
                out.push_back(r.u8());
            } else {
                auto token = r.be16();
                size_t distance = token >> 4, length = (token & 15) + 3;
                if (!distance) {
                    if (out.size() != expected)
                        throw Error("AVSLZ output size mismatch");
                    return out;
                }
                if (length > expected - out.size())
                    throw Error("AVSLZ output overflow");
                for (size_t i = 0; i < length; ++i)
                    out.push_back(distance > out.size() ? 0 : out[out.size() - distance]);
            }
        }
    }
    throw Error("AVSLZ missing terminator");
}
Bytes decodeTextureBytes(const Bytes &b, size_t expected) {
    Reader r(b);
    auto size = r.be32(), compressed = r.be32();
    if (size == expected && compressed && compressed == r.remaining())
        return decodeAvsLz(r.take(compressed), expected);
    if (size == expected && compressed == 0 && r.remaining() == expected)
        return r.take(expected);
    // Observed raw exception: first 8 pixel bytes are at the end after rotation.
    if (b.size() != expected)
        throw Error("texture size/header mismatch");
    auto raw = r.take(r.remaining());
    raw.insert(raw.end(), b.begin(), b.begin() + 8);
    return raw;
}
std::string assetMD5(const Bytes &bytes) {
    Bytes input = bytes;
    uint64_t bits = uint64_t(input.size()) * 8;
    input.push_back(0x80);
    while (input.size() % 64 != 56)
        input.push_back(0);
    for (int i = 0; i < 8; ++i)
        input.push_back(uint8_t(bits >> (8 * i)));
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    constexpr int shift[4][4] = {{7, 12, 17, 22}, {5, 9, 14, 20}, {4, 11, 16, 23}, {6, 10, 15, 21}};
    for (size_t at = 0; at < input.size(); at += 64) {
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], x[16];
        Reader r(input, at);
        for (auto &v : x)
            v = r.u32();
        for (uint32_t i = 0; i < 64; ++i) {
            uint32_t f, g;
            if (i < 16) {
                f = (b & c) | (~b & d);
                g = i;
            } else if (i < 32) {
                f = (d & b) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = b ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (b | ~d);
                g = (7 * i) % 16;
            }
            auto k = uint32_t(std::abs(std::sin(double(i + 1))) * 4294967296.0);
            auto v = a + f + k + x[g];
            auto s = shift[i / 16][i % 4];
            auto temp = d;
            d = c;
            c = b;
            b += uint32_t((v << s) | (v >> (32 - s)));
            a = temp;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
    }
    std::string out;
    constexpr char hex[] = "0123456789abcdef";
    for (auto hword : h)
        for (unsigned i = 0; i < 4; ++i) {
            auto v = uint8_t(hword >> (8 * i));
            out += hex[v >> 4];
            out += hex[v & 15];
        }
    return out;
}
std::vector<TextureInfo> textureList(const IfsArchive &a) {
    std::vector<TextureInfo> out;
    auto string = [](const Bytes &b) {
        size_t size = b.size();
        while (size && b[size - 1] == 0)
            --size;
        return std::string(b.begin(), b.begin() + size);
    };
    for (const auto &e : a.entries())
        if (e.path.find("texturelist_Exml") != std::string::npos) {
            auto doc = PropertyDocument::decode(a.read(e.path));
            auto compression = doc.root.attributes.find("compress");
            bool compressed = compression != doc.root.attributes.end();
            if (compressed && string(compression->second) != "avslz")
                throw Error("unsupported texture compression");
            for (const auto &texture : doc.root.children) {
                auto fmt = texture.attributes.find("format");
                if (fmt == texture.attributes.end() || string(fmt->second) != "argb8888rev")
                    throw Error("unsupported texture format");
                for (const auto &image : texture.children)
                    if (image.name == "image") {
                        auto n = image.attributes.find("name");
                        auto ir = image.child("imgrect"), ur = image.child("uvrect");
                        if (n == image.attributes.end() || !ir || !ur)
                            throw Error("incomplete texture mapping");
                        TextureInfo t;
                        t.nameCP932 = string(n->second);
                        t.compressed = compressed;
                        auto hash = assetMD5(Bytes(t.nameCP932.begin(), t.nameCP932.end()));
                        t.member = e.path.substr(0, e.path.rfind('/') + 1) +
                                   (hash[0] >= '0' && hash[0] <= '9' ? "_" : "") + hash;
                        auto r = ir->integers(), u = ur->integers();
                        if (r.size() != 4 || u.size() != 4)
                            throw Error("texture rect shape");
                        for (size_t i = 0; i < 4; ++i) {
                            if ((r[i] & 1) || (u[i] & 1))
                                throw Error("odd texture rect coordinate");
                            t.imageRect[i] = int32_t(r[i] / 2);
                            t.uvRect[i] = int32_t(u[i] / 2);
                        }
                        auto w = t.imageRect[1] - t.imageRect[0],
                             h = t.imageRect[3] - t.imageRect[2];
                        if (w <= 0 || h <= 0 || w > 2048 || h > 2048 ||
                            t.uvRect[0] < t.imageRect[0] || t.uvRect[1] > t.imageRect[1] ||
                            t.uvRect[2] < t.imageRect[2] || t.uvRect[3] > t.imageRect[3] ||
                            t.uvRect[0] >= t.uvRect[1] || t.uvRect[2] >= t.uvRect[3])
                            throw Error("texture rect outside image");
                        out.push_back(std::move(t));
                    }
            }
        }
    return out;
}
Bytes texturePixels(const IfsArchive &a, const TextureInfo &t) {
    size_t expected =
        size_t(t.imageRect[1] - t.imageRect[0]) * size_t(t.imageRect[3] - t.imageRect[2]) * 4;
    auto b = a.read(t.member);
    if (t.compressed)
        return decodeTextureBytes(b, expected);
    if (b.size() != expected)
        throw Error("raw texture size");
    return b;
}
Wave Wave::parse(const Bytes &b) {
    size_t start = 0;
    if (b.size() >= 4 && std::memcmp(b.data(), "SD9\0", 4) == 0) {
        Reader h(b, 4);
        start = h.u32();
    } else if (b.size() >= 24 && std::memcmp(b.data(), "RIFF", 4) != 0) {
        Reader h(b, 16);
        auto header = h.u32(), count = h.u32();
        if (count != 1 || header < 28 || header > b.size())
            throw Error("unsupported 2dx layout");
        Reader offsets(b, 72);
        auto entry = offsets.u32();
        Reader sound(b, entry);
        auto magic = sound.take(4);
        if (magic != Bytes({'2', 'D', 'X', '9'}))
            throw Error("2dx entry signature");
        auto length = sound.u32();
        start = entry + length;
    }
    Reader r(b, start);
    auto magic = r.take(4);
    if (magic != Bytes({'R', 'I', 'F', 'F'}))
        throw Error("not RIFF");
    auto length = r.u32();
    if (uint64_t(length) + 8 > b.size() - start)
        throw Error("truncated RIFF");
    auto end = start + 8 + length;
    if (r.take(4) != Bytes({'W', 'A', 'V', 'E'}))
        throw Error("not WAVE");
    Wave w;
    bool fmt = false, haveData = false;
    uint32_t fact = 0;
    while (r.position() + 8 <= end) {
        auto id = r.take(4);
        auto n = r.u32();
        if (n > end - r.position())
            throw Error("RIFF chunk exceeds container");
        auto chunk = r.take(n);
        if (n & 1)
            r.skip(1);
        if (id == Bytes({'f', 'm', 't', ' '})) {
            Reader f(chunk);
            w.format = f.u16();
            w.channels = f.u16();
            w.sampleRate = f.u32();
            f.skip(4);
            w.blockAlign = f.u16();
            w.bits = f.u16();
            if (w.format == 2) {
                auto cb = f.u16();
                if (cb < 4)
                    throw Error("ADPCM extension");
                w.samplesPerBlock = f.u16();
                auto co = f.u16();
                if (co == 0 || co > 256)
                    throw Error("ADPCM coefficient count");
                for (size_t i = 0; i < co; ++i) {
                    auto a = f.i16(), c = f.i16();
                    w.coefficients.push_back({a, c});
                }
            }
            fmt = true;
        } else if (id == Bytes({'d', 'a', 't', 'a'})) {
            if (haveData)
                throw Error("duplicate WAVE data");
            w.encoded = std::move(chunk);
            haveData = true;
        } else if (id == Bytes({'f', 'a', 'c', 't'})) {
            Reader f(chunk);
            fact = f.u32();
        }
    }
    if (!fmt || !haveData || !w.blockAlign || !w.sampleRate || w.channels != 2 ||
        w.encoded.size() % w.blockAlign)
        throw Error("unsupported/truncated WAVE layout");
    if (w.format == 1) {
        if (w.bits != 16 || w.blockAlign != 4)
            throw Error("unsupported PCM");
        w.frames = w.encoded.size() / 4;
    } else if (w.format == 2) {
        if (w.bits != 4 || w.blockAlign < 14 || w.samplesPerBlock != (w.blockAlign - 14) + 2)
            throw Error("unsupported stereo ADPCM");
        w.frames = w.blockCount() * w.samplesPerBlock;
        if (!fact || fact > w.frames)
            throw Error("invalid ADPCM fact length");
        w.frames = fact;
    } else
        throw Error("unsupported WAVE codec");
    return w;
}
std::vector<int16_t> Wave::decodeBlock(size_t index) const {
    if (index >= blockCount())
        throw Error("audio block index");
    Reader r(encoded, index * blockAlign);
    std::vector<int16_t> out;
    if (format == 1) {
        out.push_back(r.i16());
        out.push_back(r.i16());
        return out;
    }
    if (index * samplesPerBlock >= frames)
        return out;
    std::array<uint8_t, 2> predictor{r.u8(), r.u8()};
    std::array<int32_t, 2> delta{r.i16(), r.i16()}, s1{r.i16(), r.i16()}, s2{r.i16(), r.i16()};
    for (size_t c = 0; c < 2; ++c)
        if (predictor[c] >= coefficients.size() || delta[c] < 0)
            throw Error("invalid ADPCM block header");
    out = {int16_t(s2[0]), int16_t(s2[1]), int16_t(s1[0]), int16_t(s1[1])};
    constexpr int adapt[16] = {230, 230, 230, 230, 307, 409, 512, 614,
                               768, 614, 512, 409, 307, 230, 230, 230};
    for (size_t i = 14; i < blockAlign; ++i) {
        auto v = r.u8();
        for (size_t c = 0; c < 2; ++c) {
            int nib = c ? int(v & 15) : int(v >> 4), signedNib = nib >= 8 ? nib - 16 : nib;
            auto co = coefficients[predictor[c]];
            int64_t p = (int64_t(s1[c]) * co[0] + int64_t(s2[c]) * co[1]) / 256 +
                        int64_t(signedNib) * delta[c];
            auto sample = int32_t(std::clamp<int64_t>(p, -32768, 32767));
            s2[c] = s1[c];
            s1[c] = sample;
            delta[c] = int32_t(std::min<int64_t>(
                INT32_MAX, std::max<int64_t>(16, int64_t(delta[c]) * adapt[nib] / 256)));
            out.push_back(int16_t(sample));
        }
    }
    auto remaining = frames - index * samplesPerBlock;
    out.resize(size_t(std::min<uint64_t>(samplesPerBlock, remaining)) * 2);
    return out;
}
std::vector<Song> readCatalog(const Bytes &pe) {
    Reader dos(pe, 0x3c);
    auto header = dos.u32();
    Reader p(pe, header);
    if (p.u32() != 0x4550 || p.u16() != 0x14c)
        throw Error("expected x86 PE");
    auto sections = p.u16();
    p.skip(12);
    auto optSize = p.u16();
    p.skip(2);
    auto opt = p.position();
    Reader o(pe, opt);
    if (o.u16() != 0x10b)
        throw Error("expected PE32");
    Reader baseReader(pe, opt + 28);
    auto base = baseReader.u32();
    if (base != 0x10000000)
        throw Error("unexpected image base");
    size_t table = 0;
    bool found = false;
    Reader sec(pe, opt + optSize);
    for (size_t i = 0; i < sections; ++i) {
        sec.skip(8);
        auto virtualSize = sec.u32(), rva = sec.u32(), rawSize = sec.u32(), raw = sec.u32();
        sec.skip(16);
        (void)virtualSize;
        constexpr uint32_t target = 0x1b4198;
        if (target >= rva && uint64_t(target - rva) + 945 * 556 <= rawSize) {
            table = raw + target - rva;
            found = true;
        }
    }
    if (!found || table > pe.size() || 945 * 556 > pe.size() - table)
        throw Error("catalog not file-backed");
    std::vector<Song> songs;
    for (size_t i = 0; i < 945; ++i) {
        size_t at = table + i * 556;
        Song s;
        s.index = i;
        s.titleCP932 = cstring(pe, at + 56, 128);
        s.artistCP932 = cstring(pe, at + 184, 128);
        s.chartPrefix = cstring(pe, at + 485, 32);
        s.audioPrefix = cstring(pe, at + 517, 32);
        for (size_t j = 0; j < s.levels.size(); ++j)
            s.levels[j] = pe.at(at + 460 + j);
        s.timingOption = Reader(pe, at + 464).i32();
        for (size_t j = 0; j < s.audioSuffix.size(); ++j)
            s.audioSuffix[j] = pe.at(at + 549 + j);
        songs.push_back(std::move(s));
    }
    return songs;
}
std::string Song::chartPath(unsigned difficulty) const {
    if (difficulty >= 4) throw Error("invalid chart difficulty");
    constexpr const char *suffix[] = {"_b", "_m", "_h", "_w"}; // 10377480
    return "data/sound/ply2/" + chartPrefix + suffix[difficulty] + ".ply";
}
std::string Song::audioPath(unsigned difficulty) const {
    if (difficulty >= 4) throw Error("invalid audio difficulty");
    std::string result = "data/sound/2dx2/" + audioPrefix;
    if (audioSuffix[difficulty] != ' ') {
        if (audioSuffix[difficulty] < 33 || audioSuffix[difficulty] > 126 ||
            audioSuffix[difficulty] == '/' || audioSuffix[difficulty] == '\\')
            throw Error("unsupported catalog audio suffix");
        result += '_'; result += char(audioSuffix[difficulty]);
    }
    return result + ".2dx";
}
} // namespace rb
