#include "rb/AFP.hpp"
#include <algorithm>
#include <set>

namespace rb {
bool AfpPackage::contains(const std::string &path) const {
    for (const auto &entry : archive_.entries())
        if (entry.path == path)
            return true;
    return false;
}
std::string AfpPackage::resolve(const std::string &directory, const std::string &name) const {
    auto hash = assetMD5(Bytes(name.begin(), name.end()));
    const auto hashed =
        directory + "/" + (hash.front() >= '0' && hash.front() <= '9' ? "_" : "") + hash;
    if (contains(hashed))
        return hashed;
    // Also accepts original un-obfuscated members; neither form is rewritten.
    auto plain = directory + "/" + name;
    if (contains(plain))
        return plain;
    throw Error("AFP member absent: " + plain);
}
AfpPackage::AfpPackage(Bytes bytes) : archive_(std::move(bytes)) {
    if (archive_.read("magic") != Bytes({'N', 'G', 'P', 'F'}))
        throw Error("not an NGP AFP package");
    textures_ = textureList(archive_);
    for (const auto &entry : archive_.entries())
        if (entry.path.find("texturelist_Exml") != std::string::npos) {
            const auto manifest = PropertyDocument::decode(archive_.read(entry.path));
            for (const auto &sheet : manifest.root.children) {
                const auto size = sheet.child("size");
                if (!size)
                    throw Error("AFP texture sheet size absent");
                const auto values = size->integers();
                if (values.size() != 2 || values[0] <= 0 || values[1] <= 0 || values[0] > 16384 ||
                    values[1] > 16384)
                    throw Error("AFP texture sheet size invalid");
                for (const auto &image : sheet.children)
                    if (image.name == "image") {
                        const auto attr = image.attributes.find("name");
                        if (attr == image.attributes.end())
                            throw Error("AFP image name absent");
                        auto bytes = attr->second;
                        while (!bytes.empty() && !bytes.back())
                            bytes.pop_back();
                        textureSheetSizes_.emplace(
                            std::string(bytes.begin(), bytes.end()),
                            std::array<int32_t, 2>{int32_t(values[0]), int32_t(values[1])});
                    }
            }
        }
    std::set<std::string> names;
    for (const auto &t : textures_)
        if (!names.insert(t.nameCP932).second)
            throw Error("duplicate AFP bitmap name");
    // Raw IFS node encoding escapes the dot in afplist.xml as _E.
    if (!contains("afp/afplist_Exml"))
        return; // Valid bitmap-only packages have no movie table.
    const auto doc = PropertyDocument::decode(archive_.read("afp/afplist_Exml"));
    if (doc.root.name != "afplist")
        throw Error("AFP movie table root");
    names.clear();
    for (const auto &node : doc.root.children) {
        if (node.name != "afp")
            throw Error("unknown AFP movie table entry");
        auto attr = node.attributes.find("name");
        if (attr == node.attributes.end() || attr->second.empty())
            throw Error("AFP movie name missing");
        AfpMovieInfo info;
        const auto &v = attr->second;
        size_t length = v.size();
        while (length && v[length - 1] == 0)
            --length;
        info.name.assign(v.begin(), v.begin() + length);
        if (info.name.empty() || !names.insert(info.name).second)
            throw Error("empty/duplicate AFP movie name");
        info.member = resolve("afp", info.name);
        // BSI accompanies each inspected game movie, but its execution has not
        // been restored. A missing stream stays explicit on access.
        try {
            info.bitStreamMember = resolve("afp/bsi", info.name);
        } catch (const Error &) {
        }
        for (const auto &child : node.children) {
            if (child.name != "geo" || child.type != 5)
                throw Error("unsupported AFP geometry table");
            for (const auto id : child.integers()) {
                if (id < 0 || id > 65535)
                    throw Error("AFP geometry identifier range");
                info.shapes.push_back(static_cast<uint16_t>(id));
                resolve("geo", info.name + "_shape" + std::to_string(id));
            }
        }
        movies_.push_back(std::move(info));
    }
}
const TextureInfo &AfpPackage::texture(const std::string &name) const {
    for (const auto &t : textures_)
        if (t.nameCP932 == name)
            return t;
    for (const auto *fallback : textureFallbacks_)
        for (const auto &t : fallback->textures()) if (t.nameCP932 == name) return t;
    throw Error("AFP bitmap absent: " + name);
}
const AfpMovieInfo &AfpPackage::movie(const std::string &name) const {
    for (const auto &m : movies_)
        if (m.name == name)
            return m;
    throw Error("AFP movie absent: " + name);
}
std::array<int32_t, 2> AfpPackage::textureSheetSize(const std::string &name) const {
    auto found = textureSheetSizes_.find(name);
    if (found == textureSheetSizes_.end()) {
        for (const auto *fallback : textureFallbacks_)
            for (const auto &t : fallback->textures()) if (t.nameCP932 == name) return fallback->textureSheetSize(name);
        throw Error("AFP texture sheet absent");
    }
    return found->second;
}
AfpBitmap AfpPackage::bitmap(const std::string &name) const {
    if (std::none_of(textures_.begin(), textures_.end(), [&](const auto &t){return t.nameCP932 == name;}))
        for (const auto *fallback : textureFallbacks_)
            for (const auto &t : fallback->textures()) if (t.nameCP932 == name) return fallback->bitmap(name);
    AfpBitmap out;
    out.texture = texture(name);
    const auto &t = out.texture;
    const int32_t stride = t.imageRect[1] - t.imageRect[0];
    const auto pixels = texturePixels(archive_, t);
    out.width = t.uvRect[1] - t.uvRect[0];
    out.height = t.uvRect[3] - t.uvRect[2];
    out.bgra.reserve(size_t(out.width) * size_t(out.height) * 4);
    for (int32_t y = t.uvRect[2]; y < t.uvRect[3]; ++y) {
        const size_t at =
            (size_t(y - t.imageRect[2]) * size_t(stride) + size_t(t.uvRect[0] - t.imageRect[0])) *
            4;
        out.bgra.insert(out.bgra.end(), pixels.begin() + at,
                        pixels.begin() + at + size_t(out.width) * 4);
    }
    return out;
}
Bytes AfpPackage::movieBytes(const std::string &name) const {
    return archive_.read(movie(name).member);
}
Bytes AfpPackage::bitStreamBytes(const std::string &name) const {
    const auto &path = movie(name).bitStreamMember;
    if (path.empty())
        throw Error("AFP BSI stream absent: " + name);
    return archive_.read(path);
}
Bytes AfpPackage::geometryBytes(const std::string &name, uint16_t shape) const {
    const auto &m = movie(name);
    if (std::find(m.shapes.begin(), m.shapes.end(), shape) == m.shapes.end())
        throw Error("AFP shape absent from movie table");
    return archive_.read(resolve("geo", name + "_shape" + std::to_string(shape)));
}
} // namespace rb
