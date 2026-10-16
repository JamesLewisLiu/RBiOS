#pragma once
#include "Assets.hpp"

namespace rb {
// Archive-level binding, not an AFP bytecode interpreter. Names retain the
// archive's original encoding. No converted assets or persistent cache.
struct AfpMovieInfo {
    std::string name, member, bitStreamMember;
    std::vector<uint16_t> shapes;
};
struct AfpBitmap {
    TextureInfo texture;
    int32_t width = 0, height = 0;
    Bytes bgra; // visible UV rectangle, no border padding
};
class AfpPackage {
    IfsArchive archive_;
    std::vector<TextureInfo> textures_;
    std::vector<AfpMovieInfo> movies_;
    std::map<std::string, std::array<int32_t, 2>> textureSheetSizes_;
    bool contains(const std::string &) const;
    std::string resolve(const std::string &directory, const std::string &name) const;

  public:
    explicit AfpPackage(Bytes);
    const std::vector<TextureInfo> &textures() const {
        return textures_;
    }
    const std::vector<AfpMovieInfo> &movies() const {
        return movies_;
    }
    const TextureInfo &texture(const std::string &originalName) const;
    const AfpMovieInfo &movie(const std::string &originalName) const;
    std::array<int32_t, 2> textureSheetSize(const std::string &originalName) const;
    AfpBitmap bitmap(const std::string &originalName) const;
    Bytes movieBytes(const std::string &originalName) const;
    Bytes bitStreamBytes(const std::string &originalName) const;
    Bytes geometryBytes(const std::string &movieName, uint16_t shape) const;
};
} // namespace rb
