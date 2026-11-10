#include "rb/AfpTimeline.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace rb {
namespace {
uint32_t at32(const Bytes &b, size_t at) {
    return Reader(b, at).u32();
}
uint16_t at16(const Bytes &b, size_t at) {
    return Reader(b, at).u16();
}
AfpMatrix combine(const AfpMatrix &p, const AfpMatrix &q) {
    return {p.a * q.a + p.c * q.b, p.b * q.a + p.d * q.b,          p.a * q.c + p.c * q.d,
            p.b * q.c + p.d * q.d, p.a * q.tx + p.c * q.ty + p.tx, p.b * q.tx + p.d * q.ty + p.ty};
}
} // namespace
AfpTimeline::AfpTimeline(Bytes movie, const Bytes &bsi) : decoded_(std::move(movie)) {
    Reader swaps(bsi);
    size_t at = 0;
    bool terminated = false;
    while (swaps.remaining()) {
        const auto word = swaps.u16();
        if (!word) {
            terminated = true;
            break;
        }
        const unsigned type = word >> 13, loops = (word >> 7) & 63;
        at += size_t(word & 127) * 2;
        if (!type) {
            at += size_t(loops) * 256;
            if (at > decoded_.size())
                throw Error("AFP BSI jump range");
            continue;
        }
        if (type > 3)
            throw Error("unsupported AFP BSI swap type");
        const size_t width = size_t(1) << type;
        for (unsigned i = 0; i <= loops; ++i) {
            Reader(decoded_, at).require(width);
            std::reverse(decoded_.begin() + at, decoded_.begin() + at + width);
            at += width;
        }
    }
    if (!terminated)
        throw Error("AFP BSI terminator absent");
    Reader r(decoded_);
    const auto magic = r.u32();
    if ((magic & 0x7f7f7f00) != 0x41503200 || (magic & 255) < 7 || (magic & 255) > 10)
        throw Error("unsupported AP2 magic/version");
    if (r.u32() != decoded_.size() || r.u16() != 0x200)
        throw Error("AFP header length/data version");
    const auto nameOffset = r.u16();
    const auto flags = r.u32();
    for (auto &v : location)
        v = r.u16();
    fps = (flags & 2) ? float(r.i32()) / 1024.f : r.f32();
    if (!std::isfinite(fps) || fps <= 0)
        throw Error("AFP frame rate");
    strings_ = at32(decoded_, 48);
    stringSize_ = at32(decoded_, 52);
    Reader(decoded_, strings_).require(stringSize_);
    for (size_t i = 0; i < stringSize_; ++i)
        decoded_[strings_ + i] = uint8_t(decoded_[strings_ + i] - uint8_t(128 + i));
    name = stringAt(nameOffset);
    const auto count = at16(decoded_, 32);
    const size_t ex = at32(decoded_, 40);
    Reader(decoded_, ex).require(size_t(count) * 4);
    for (size_t i = 0; i < count; ++i) {
        auto symbol = stringAt(at16(decoded_, ex + 4 * i + 2));
        if (!exports.emplace(symbol, at16(decoded_, ex + 4 * i)).second)
            throw Error("duplicate AFP export");
    }
    root = parseTimeline(at32(decoded_, 36), 0);
    index(*root);
}
std::string AfpTimeline::stringAt(uint16_t off) const {
    if (!off)
        return {};
    if (off >= stringSize_)
        throw Error("AFP string outside table");
    return cstring(decoded_, strings_ + off, stringSize_ - off);
}
std::shared_ptr<AfpTag::Timeline> AfpTimeline::parseTimeline(size_t base, unsigned depth) {
    if (depth > 32)
        throw Error("AFP sprite recursion limit");
    Reader r(decoded_, base);
    r.u16();
    const auto labelCount = r.u16();
    const auto frameCount = r.u32(), tagCount = r.u32();
    const auto labels = r.u32(), frames = r.u32(), tags = r.u32();
    if (frameCount > 100000 || tagCount > 100000)
        throw Error("AFP timeline limit");
    auto out = std::make_shared<AfpTag::Timeline>();
    Reader fr(decoded_, base + frames);
    for (uint32_t i = 0; i < frameCount; ++i) {
        auto bits = fr.u32();
        AfpTag::Timeline::Frame f{bits & 0xfffff, bits >> 20};
        if (f.first > tagCount || f.count > tagCount - f.first)
            throw Error("AFP frame tag range");
        out->frames.push_back(f);
    }
    Reader lr(decoded_, base + labels);
    for (unsigned i = 0; i < labelCount; ++i) {
        auto frame = lr.u16();
        auto label = stringAt(lr.u16());
        if (frame >= frameCount || !out->labels.emplace(label, frame).second)
            throw Error("AFP label/frame");
    }
    Reader tr(decoded_, base + tags);
    for (uint32_t i = 0; i < tagCount; ++i) {
        auto head = tr.u32();
        AfpTag tag;
        tag.kind = uint16_t(head >> 22);
        tag.offset = tr.position();
        auto size = head & 0x3fffff;
        if (size > 0x200000)
            throw Error("AFP tag size limit");
        tag.payload = tr.take(size);
        tr.skip((4 - size % 4) % 4);
        if (tag.kind == 0x79) {
            auto flags = at16(tag.payload, 0);
            tag.id = at16(tag.payload, 2);
            tag.sprite =
                parseTimeline(tag.offset + ((flags & 1) ? at32(tag.payload, 4) : 4), depth + 1);
        } else if (tag.kind == 0x84) {
            if (size != 4)
                throw Error("AFP shape tag size");
            tag.id = at16(tag.payload, 2);
        }
        out->tags.push_back(std::move(tag));
    }
    return out;
}
void AfpTimeline::index(const AfpTag::Timeline &timeline) {
    for (const auto &t : timeline.tags) {
        if (t.id && !definitions_.emplace(t.id, &t).second)
            throw Error("duplicate AFP definition id");
        if (t.sprite)
            index(*t.sprite);
    }
}
AfpPlacement AfpTimeline::placement(const AfpTag &tag) const {
    if (tag.kind != 0x7f)
        throw Error("AFP tag is not placement");
    Reader r(tag.payload);
    AfpPlacement p;
    p.flags = r.u32();
    p.depth = r.u16();
    p.object = r.u16();
    constexpr uint32_t supported = 0x1 | 0x2 | 0x4 | 0x8 | 0x20 | 0x80 | 0x100 | 0x200 | 0x400 |
                                   0x800 | 0x1000 | 0x2000 | 0x4000 | 0x20000 | 0x40000 | 0x80000 |
                                   0x1000000 | 0x2000000;
    if (p.flags & ~supported)
        throw Error("unsupported AFP placement flags: " + std::to_string(p.flags));
    if (p.flags & 2)
        p.source = r.u16();
    if (p.flags & 0x20)
        p.instanceName = stringAt(r.u16());
    if (p.flags & 0x20000)
        p.blend = r.u8();
    r.skip((4 - r.position() % 4) % 4);
    if (p.flags & 0x100) {
        p.matrix.a = float(r.i32()) / 1024;
        p.matrix.d = float(r.i32()) / 1024;
    }
    if (p.flags & 0x200) {
        p.matrix.b = float(r.i32()) / 1024;
        p.matrix.c = float(r.i32()) / 1024;
    }
    if (p.flags & 0x400) {
        p.matrix.tx = float(r.i32()) / 20;
        p.matrix.ty = float(r.i32()) / 20;
    }
    for (auto entry :
         {std::pair<uint32_t, std::array<float, 4> *>(0x800, &p.multiply), {0x1000, &p.addition}})
        if (p.flags & entry.first)
            for (auto &v : *entry.second)
                v = float(r.i16()) / 255;
    for (auto entry :
         {std::pair<uint32_t, std::array<float, 4> *>(0x2000, &p.multiply), {0x4000, &p.addition}})
        if (p.flags & entry.first) {
            auto c = r.u32();
            for (unsigned i = 0; i < 4; ++i)
                (*entry.second)[i] = float((c >> (24 - 8 * i)) & 255) / 255;
        }
    if (p.flags & 0x80) {
        const size_t start = r.position();
        const auto events = r.u32(), size = r.u32();
        if (size < 8)
            throw Error("AFP event block size");
        Reader(tag.payload, start).require(size);
        if (events) {
            r.u16();
            const auto count = r.u16();
            for (unsigned i = 0; i < count; ++i) {
                const auto entry = start + 12 + 8 * i;
                Reader er(tag.payload, entry);
                if (er.u32() != 1 || er.u8() != 0 || er.u8() != 0)
                    throw Error("unsupported AFP event trigger");
                const auto offset = entry + er.u16();
                const auto end = i + 1 < count ? start + 12 + 8 * (i + 1) +
                                                     at16(tag.payload, start + 12 + 8 * (i + 1) + 6)
                                               : start + size;
                if (offset > end || end > start + size)
                    throw Error("AFP event bytecode range");
                auto effect =
                    script(Bytes(tag.payload.begin() + offset, tag.payload.begin() + end));
                if (effect.stop || !effect.go.empty() ||
                    (effect.reportedFrame < 0 && !effect.rectangularMask))
                    throw Error("unsupported AFP object event effect");
                p.reportedFrame = effect.reportedFrame;
                if (effect.rectangularMask)
                    p.rectangularMask = effect.rectangularMask;
            }
        }
        if (r.position() > start + size)
            throw Error("AFP event block overrun");
        r.skip(start + size - r.position());
    }
    if (p.flags & 0x1000000) {
        p.originX = float(r.i32()) / 20;
        p.originY = float(r.i32()) / 20;
    }
    if (p.flags & 0x2000000)
        p.originX = p.originY = 0;
    if (p.flags & 0x40000) {
        p.matrix.a = float(r.i16()) / 32768;
        p.matrix.d = float(r.i16()) / 32768;
    }
    if (p.flags & 0x80000) {
        p.matrix.b = float(r.i16()) / 32768;
        p.matrix.c = float(r.i16()) / 32768;
    }
    r.skip((4 - r.position() % 4) % 4);
    if (r.remaining())
        throw Error("AFP placement unconsumed bytes");
    return p;
}
std::vector<AfpSpriteCommand> AfpTimeline::sampleExportedFrame(const AfpPackage &package,
                                                               const std::string &symbol,
                                                               uint32_t frame,
                                                               const AfpSampleOptions &options) const {
    auto ex = exports.find(symbol);
    if (ex == exports.end())
        throw Error("AFP export absent");
    std::vector<AfpSpriteCommand> out;
    std::set<uint16_t> active;
    std::function<void(uint16_t, uint32_t, const AfpMatrix &, std::array<float, 4>, unsigned,
                       std::vector<AfpClipQuad>, std::string, std::string,
                       std::optional<uint32_t>)>
        visit;
    visit = [&](uint16_t id, uint32_t tick, const AfpMatrix &parent, std::array<float, 4> color,
                unsigned blend, std::vector<AfpClipQuad> clips, std::string path,
                std::string binding, std::optional<uint32_t> playback) {
        if (!active.insert(id).second)
            throw Error("AFP active symbol cycle");
        auto def = definitions_.find(id);
        if (def == definitions_.end())
            throw Error("unsupported AFP imported symbol: " + std::to_string(id));
        const auto &tag = *def->second;
        if (tag.sprite) {
            const auto &tl = *tag.sprite;
            if (tl.frames.empty() || tick >= tl.frames.size())
                throw Error("AFP frame outside symbol timeline");
            struct Object {
                AfpPlacement p;
                uint32_t born;
            };
            std::map<uint16_t, Object> display;
            for (uint32_t f = 0; f <= tick; ++f) {
                auto range = tl.frames[f];
                for (uint32_t i = range.first; i < range.first + range.count; ++i) {
                    const auto &cmd = tl.tags[i];
                    if (cmd.kind == 0x79 || cmd.kind == 0x84)
                        continue;
                    if (cmd.kind == 0x7f) {
                        auto p = placement(cmd);
                        auto found = display.find(p.depth);
                        if (p.flags & 1) {
                            if (found == display.end() || found->second.p.object != p.object)
                                throw Error("AFP updates absent object");
                            auto &old = found->second.p;
                            if (p.flags & 2)
                                old.source = p.source;
                            if (p.flags & 4) {
                                if (p.flags & (0x100 | 0x40000)) {
                                    old.matrix.a = p.matrix.a;
                                    old.matrix.d = p.matrix.d;
                                }
                                if (p.flags & (0x200 | 0x80000)) {
                                    old.matrix.b = p.matrix.b;
                                    old.matrix.c = p.matrix.c;
                                }
                                if (p.flags & 0x400) {
                                    old.matrix.tx = p.matrix.tx;
                                    old.matrix.ty = p.matrix.ty;
                                }
                                if (!(p.flags & (0x100 | 0x200 | 0x400 | 0x40000 | 0x80000)))
                                    old.matrix = p.matrix;
                            }
                            if (p.flags & 8) {
                                old.multiply = p.multiply;
                                old.addition = p.addition;
                            }
                            if (p.flags & 0x20000)
                                old.blend = p.blend;
                            if (p.flags & 0x3000000) {
                                old.originX = p.originX;
                                old.originY = p.originY;
                            }
                            if (p.rectangularMask)
                                old.rectangularMask = p.rectangularMask;
                        } else
                            display[p.depth] = {p, f};
                    } else if (cmd.kind == 0x80) {
                        Reader q(cmd.payload);
                        q.u16();
                        display.erase(q.u16());
                    } else if (cmd.kind == 0x7a) {
                        script(cmd.payload); // seeking a fixed frame validates, but does not
                                             // advance playback
                    } else
                        throw Error("unsupported AFP execution tag: " + std::to_string(cmd.kind) +
                                    " at " + std::to_string(cmd.offset));
                }
            }
            for (const auto &[depth, obj] : display) {
                (void)depth;
                const auto &p = obj.p;
                if (options.hiddenObjects.count(p.instanceName))
                    continue;
                for (float v : p.addition)
                    if (v != 0)
                        throw Error("unsupported AFP additive color");
                std::array<float, 4> c;
                for (unsigned i = 0; i < 4; ++i)
                    c[i] = color[i] * p.multiply[i];
                AfpMatrix matrix = p.matrix;
                matrix.tx -= matrix.a * p.originX + matrix.c * p.originY;
                matrix.ty -= matrix.b * p.originX + matrix.d * p.originY;
                auto target = definitions_.find(p.source);
                const auto elapsed = playback.value_or(tick);
                if (elapsed < obj.born)
                    throw Error("AFP playback precedes object creation");
                uint32_t age = elapsed - obj.born;
                const auto childPlayback = playback ? std::optional<uint32_t>(age) : std::nullopt;
                if (target != definitions_.end() && target->second->sprite) {
                    auto count = target->second->sprite->frames.size();
                    if (count)
                        age %= count;
                }
                const auto childMatrix = combine(parent, matrix);
                auto childClips = clips;
                if (p.rectangularMask) {
                    const auto &r = *p.rectangularMask;
                    AfpClipQuad quad{{{r[0], r[3]}, {r[1], r[3]}, {r[0], r[2]}, {r[1], r[2]}}};
                    for (auto &point : quad) {
                        const float x = point[0], y = point[1];
                        point = {childMatrix.a * x + childMatrix.c * y + childMatrix.tx,
                                 childMatrix.b * x + childMatrix.d * y + childMatrix.ty};
                    }
                    childClips.push_back(quad);
                }
                visit(p.source, age, childMatrix, c, p.blend ? p.blend : blend,
                      std::move(childClips), path + (p.instanceName.empty() ? "" : "/" + p.instanceName),
                      p.instanceName.empty() ? binding : p.instanceName, childPlayback);
            }
        } else if (tag.kind == 0x84) {
            if (color[3] == 0) {
                active.erase(id);
                return;
            } // original fully transparent geometry
            const auto geo = package.geometryBytes(name, id);
            Reader r(geo);
            if (r.be32() != 0x47453244)
                throw Error("unsupported AFP geometry endian");
            r.skip(8);
            if (r.be32() != geo.size())
                throw Error("AFP geometry size");
            const auto flags = r.be32();
            if (flags & ~12u)
                throw Error("unsupported AFP geometry flags");
            auto vertices = r.be16();
            auto textureCoordinates = r.be16();
            r.be16();
            const auto labels = r.be16(), draws = r.be16();
            r.be16();
            auto vertexOffset = r.be32();
            auto textureOffset = r.be32();
            r.be32();
            auto labelOffset = r.be32(), drawOffset = r.be32();
            if (vertices != 4 || draws != 1)
                throw Error("unsupported AFP non-rectangle geometry");
            Reader vr(geo, vertexOffset);
            std::array<std::array<float, 2>, 4> pts;
            for (auto &pt : pts)
                for (auto &v : pt) {
                    auto bits = vr.be32();
                    std::memcpy(&v, &bits, 4);
                }
            if (pts[0][1] != pts[1][1] || pts[0][0] != pts[2][0] || pts[2][1] != pts[3][1] ||
                pts[1][0] != pts[3][0])
                throw Error("unsupported AFP irregular quad");
            Reader dr(geo, drawOffset);
            auto mode = dr.u8(), df = dr.u8(), texture = dr.u8(), tex2 = dr.u8();
            if (mode != 4 || (df != 3 && df != 67 && df != 9) || tex2 != 255 || (df != 9 && texture >= labels))
                throw Error("unsupported AFP untextured/colored geometry id=" + std::to_string(id) + " flags=" + std::to_string(df));
            if (dr.be16() != 6 || dr.be16() != 0)
                throw Error("unsupported AFP triangles");
            const auto rgba = dr.be32();
            auto triangles = dr.be32();
            Reader ir(geo, triangles);
            for (auto expected : {0, 1, 2, 2, 1, 3})
                if (ir.be16() != expected)
                    throw Error("unsupported AFP triangle topology");
            if (df == 9) {
                if (texture != 255 || textureCoordinates != 0)
                    throw Error("unsupported AFP solid rectangle texture fields");
                const std::array<float,4> solid{{float((rgba >> 24) & 255) / 255,
                    float((rgba >> 16) & 255) / 255, float((rgba >> 8) & 255) / 255,
                    float(rgba & 255) / 255}};
                out.push_back({"", parent, {pts[0][0], pts[1][0], pts[0][1], pts[2][1]},
                    color, {}, uint8_t(blend), clips, path, solid});
                active.erase(id);
                return;
            }
            Reader lr(geo, labelOffset + 4 * texture);
            auto text = lr.be32();
            auto raw = cstring(geo, text, geo.size() - text);
            std::string region;
            const bool scrambled = !raw.empty() && uint8_t(raw[0]) > 159;
            for (const auto ch : raw)
                region += char(uint8_t(ch) + (scrambled ? 128 : 0));
            const auto &textureInfo = package.texture(region);
            const auto sheetSize = package.textureSheetSize(region);
            if (textureCoordinates != 4 || !textureOffset)
                throw Error("unsupported AFP UV count");
            Reader uv(geo, textureOffset);
            for (unsigned vertex = 0; vertex < 4; ++vertex)
                for (unsigned axis = 0; axis < 2; ++axis) {
                    const auto bits = uv.be32();
                    float value;
                    std::memcpy(&value, &bits, 4);
                    const auto edge = axis == 0 ? (vertex % 2) : (2 + vertex / 2);
                    const auto expected = df == 67 ? float(vertex / (axis == 0 ? 1 : 2) % 2)
                        : float(textureInfo.uvRect[edge]) / float(sheetSize[axis]);
                    if (!std::isfinite(value) || std::abs(value - expected) > 0.0000001f)
                        throw Error("unsupported AFP texture UV variation");
                }
            auto replacement = options.textureOverrides.find(binding);
            if (replacement != options.textureOverrides.end()) {
                const auto &target = package.texture(replacement->second);
                if (target.uvRect[1] - target.uvRect[0] != textureInfo.uvRect[1] - textureInfo.uvRect[0] ||
                    target.uvRect[3] - target.uvRect[2] != textureInfo.uvRect[3] - textureInfo.uvRect[2])
                    throw Error("AFP replacement requires original texture geometry");
                region = replacement->second;
            }
            out.push_back({region,
                           parent,
                           {pts[0][0], pts[1][0], pts[0][1], pts[2][1]},
                           color,
                           {},
                           uint8_t(blend), clips, path, std::nullopt});
        } else
            throw Error("unsupported AFP symbol kind");
        active.erase(id);
    };
    visit(ex->second, frame, AfpMatrix{}, {1, 1, 1, 1}, 0, {}, symbol, "", options.playbackTicks);
    return out;
}
AfpTimeline::ScriptEffect AfpTimeline::script(const Bytes &bytes) const {
    Reader r(bytes);
    if (r.u8() != 255)
        throw Error("unsupported SWF bytecode");
    const auto flags = r.u8();
    if (flags & ~1)
        throw Error("unsupported AFP script header");
    std::vector<uint16_t> strings;
    if (flags & 1) {
        auto count = r.u16();
        for (unsigned i = 0; i < count; ++i)
            strings.push_back(r.u16());
    }
    struct Value {
        enum Kind { Integer, This, Constant, Text, Library, Undefined } kind;
        int32_t n = 0;
        std::string text;
    };
    std::vector<Value> stack;
    auto pop = [&]() {
        if (stack.empty())
            throw Error("AFP script stack underflow");
        auto v = stack.back();
        stack.pop_back();
        return v;
    };
    ScriptEffect effect;
    while (r.remaining()) {
        const auto op = r.u8();
        if (op == 0)
            break;
        if (op == 0x43) {
            auto count = r.u8();
            for (unsigned i = 0; i < count; ++i) {
                const auto type = r.u8();
                if (type == 0)
                    stack.push_back({Value::Integer, 0, {}});
                else if (type == 7)
                    stack.push_back({Value::Integer, int32_t(r.be32()), {}});
                else if (type == 12)
                    stack.push_back({Value::This, 0, {}});
                else if (type == 0x13)
                    stack.push_back({Value::Constant, 0x300 + r.u8(), {}});
                else if (type == 0x12)
                    stack.push_back({Value::Constant, 0x200 + r.u8(), {}});
                else if (type == 0x16)
                    stack.push_back({Value::Constant, 0x400 + r.u8(), {}});
                else if (type == 0x27)
                    stack.push_back({Value::Constant, 0x800 + r.u8(), {}});
                else if (type == 8) {
                    auto index = r.u8();
                    if (index >= strings.size())
                        throw Error("AFP script string index");
                    stack.push_back({Value::Text, 0, stringAt(strings[index])});
                } else
                    throw Error("unsupported AFP push type: " + std::to_string(type));
            }
        } else if (op == 0x0e) {
            auto v = pop();
            if (v.kind != Value::Constant || v.n != 0x390)
                throw Error("unsupported AFP variable lookup");
            stack.push_back({Value::Library, 0, {}});
        } else if (op == 0x32) {
            const auto method = pop(), object = pop(), count = pop();
            if (method.kind != Value::Constant || object.kind != Value::Library ||
                count.kind != Value::Integer || count.n < 1 || count.n > 5)
                throw Error("unsupported AFP method call");
            const auto self = pop();
            if (self.kind != Value::This)
                throw Error("unsupported AFP script target");
            if (method.n == 0x440 && count.n == 1)
                effect.stop = true;
            else if ((method.n == 0x815 || method.n == 0x442) && count.n == 2) {
                auto target = pop();
                if (target.kind == Value::Constant && target.n == 0x20e)
                    target = {Value::Text, 0, "normal"};
                if (target.kind != Value::Text)
                    throw Error("AFP goto label type");
                effect.go = target.text;
            } else if (method.n == 0x836 && count.n == 2) {
                auto frame = pop();
                if (frame.kind != Value::Integer)
                    throw Error("AFP frame callback type");
                effect.reportedFrame = frame.n;
            } else if (method.n == 0x833 && count.n == 5) {
                std::array<float, 4> rect;
                for (auto &value : rect) {
                    auto argument = pop();
                    if (argument.kind != Value::Integer)
                        throw Error("AFP rectangle mask argument type");
                    value = float(argument.n);
                }
                if (rect[0] >= rect[1] || rect[2] >= rect[3])
                    throw Error("AFP rectangle mask range");
                effect.rectangularMask = rect;
            } else
                throw Error("unsupported AFP aeplib method: " + std::to_string(method.n));
            stack.push_back({Value::Undefined, 0, {}});
        } else if (op == 0x0d)
            pop();
        else
            throw Error("unsupported AFP opcode: " + std::to_string(op));
    }
    // Known scripts pad with END; any nonzero unexecuted bytes are rejected.
    while (r.remaining())
        if (r.u8() != 0)
            throw Error("AFP trailing script data");
    return effect;
}
std::vector<AfpSpriteCommand> AfpTimeline::sampleExportedLabelFrame(const AfpPackage &package,
                                                                    const std::string &symbol,
                                                                    const std::string &label,
                                                                    uint32_t elapsed) const {
    return sampleExportedFrame(package, symbol, resolveExportedLabelFrame(symbol, label, elapsed));
}
uint32_t AfpTimeline::resolveExportedLabelFrame(const std::string &symbol, const std::string &label,
                                                uint32_t elapsed) const {
    auto ex = exports.find(symbol);
    if (ex == exports.end())
        throw Error("AFP export absent");
    auto def = definitions_.find(ex->second);
    if (def == definitions_.end() || !def->second->sprite)
        throw Error("AFP export is not sprite");
    const auto &tl = *def->second->sprite;
    auto start = tl.labels.find(label);
    if (start == tl.labels.end())
        throw Error("AFP label absent: " + label);
    if (elapsed > 1000000)
        throw Error("AFP playback tick limit");
    // AFP movie APIs use 1-based frame numbers; display-list indices are 0-based.
    if (!start->second)
        throw Error("AFP zero frame label unsupported");
    uint32_t frame = start->second - 1;
    bool stopped = false;
    std::map<uint32_t, uint32_t> visited;
    for (uint32_t tick = 0;; ++tick) {
        unsigned jumps = 0;
        for (;;) {
            bool stop = false;
            std::string go;
            const auto range = tl.frames[frame];
            for (uint32_t i = range.first; i < range.first + range.count; ++i)
                if (tl.tags[i].kind == 0x7a) {
                    auto e = script(tl.tags[i].payload);
                    stop |= e.stop;
                    if (!e.go.empty())
                        go = e.go;
                }
            if (go.empty()) {
                stopped |= stop;
                break;
            }
            auto next = tl.labels.find(go);
            if (next == tl.labels.end() || !next->second)
                throw Error("AFP goto label absent/zero");
            frame = next->second - 1;
            stopped = false;
            if (++jumps > 64)
                throw Error("AFP same-tick script jump limit");
        }
        const auto prior = visited.emplace(frame, tick);
        if (!stopped && !prior.second) {
            const auto period = tick - prior.first->second;
            if (period)
                tick += ((elapsed - tick) / period) * period;
        }
        if (tick == elapsed || stopped)
            break;
        frame = (frame + 1) % uint32_t(tl.frames.size());
    }
    return frame;
}
SwitchMovieSample prepareSwitchMovieDraw(const AfpTimeline &timeline, const std::string &symbol,
                                         SwitchMovieState &state, int64_t tick, bool held) {
    uint32_t elapsed = 0, frame = 0;
    if (!state.label.empty()) {
        if (tick < state.labelTick || uint64_t(tick - state.labelTick) > 1000000)
            throw Error("switch movie clock range");
        elapsed = uint32_t(tick - state.labelTick);
        frame = timeline.resolveExportedLabelFrame(symbol, state.label, elapsed);
    }
    // Play(rate1) changes layer speed/visibility, not the selected frame.
    state.layerPlaying = true;
    state.visible = true;
    const auto on = timeline.resolveExportedLabelFrame(symbol, "on", 0);
    // 10066430 compares movie API frame numbers; both sides use the same basis.
    if (!held || frame < on) {
        state.label = held ? "on" : "off";
        state.labelTick = tick;
        elapsed = 0;
        frame = timeline.resolveExportedLabelFrame(symbol, state.label, 0);
    }
    return {state.label, elapsed, frame};
}
} // namespace rb
