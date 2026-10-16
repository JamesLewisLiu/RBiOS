#pragma once
#include "Property.hpp"
#include <array>
#include <functional>

namespace rb {
struct IfsEntry {
    std::string path;
    size_t offset = 0, size = 0;
};
class IfsArchive {
    Bytes bytes_;
    size_t base_ = 0;
    std::vector<IfsEntry> entries_;

  public:
    explicit IfsArchive(Bytes);
    const std::vector<IfsEntry> &entries() const {
        return entries_;
    }
    Bytes read(const std::string &) const;
};
Bytes decodeAvsLz(const Bytes &, size_t expected, size_t limit = 64 * 1024 * 1024);
Bytes decodeTextureBytes(const Bytes &, size_t expected);
std::string assetMD5(const Bytes &);
struct TextureInfo {
    std::string nameCP932, member;
    std::array<int32_t, 4> imageRect{}, uvRect{};
    bool compressed = false;
};
std::vector<TextureInfo> textureList(const IfsArchive &);
Bytes texturePixels(const IfsArchive &, const TextureInfo &);
struct Wave {
    uint16_t format = 0, channels = 0, blockAlign = 0, bits = 0, samplesPerBlock = 0;
    uint32_t sampleRate = 0;
    uint64_t frames = 0;
    std::vector<std::array<int16_t, 2>> coefficients;
    Bytes encoded;
    static Wave parse(const Bytes &wrapper);
    // Decode one block, suitable for streaming/seek. PCM frames are interleaved.
    std::vector<int16_t> decodeBlock(size_t index) const;
    size_t blockCount() const {
        return encoded.size() / blockAlign;
    }
};
struct Song {
    size_t index = 0;
    std::string titleCP932, artistCP932, chartPrefix, audioPrefix;
    std::array<uint8_t, 4> levels{}; // original signed bytes at catalog +460
    int32_t timingOption = 0; // catalog +464, copied to CMusicSheet2 +84; not stage end time
    std::array<uint8_t, 4> audioSuffix{}; // +549, space means unsuffixed
    std::string chartPath(unsigned difficulty) const; // 0x1008DB80
    std::string audioPath(unsigned difficulty) const; // 0x1008DAF0
};
std::vector<Song> readCatalog(const Bytes &pe);
} // namespace rb
