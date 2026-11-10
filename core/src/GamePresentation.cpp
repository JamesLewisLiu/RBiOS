#include "rb/GamePresentation.hpp"
#include "rb/OriginalAfpMoviePool.hpp"
#include "rb/StageGraphic.hpp"
#include "rb/StageScoreGraphic.hpp"
#include "rb/StageLabelGraphic.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <set>

namespace rb {
std::array<MotionVector, 4> originalSpriteQuad(float width, float height, float x, float y,
                                               float angle, float sx, float sy, float ax,
                                               float ay) {
    std::array<MotionVector, 4> result{{{0, height}, {width, height}, {0, 0}, {width, 0}}};
    const float c = std::cos(angle), s = std::sin(angle);
    for (auto &p : result) {
        const float dx = (p.x - width * ax) * sx, dy = (p.y - height * ay) * sy;
        p = {x + c * dx - s * dy, y + s * dx + c * dy};
    }
    return result;
}
struct GamePresentation::Impl {
    struct Movie {
        SessionMovieBinding binding;
        std::optional<uint32_t> poolSlot;
        uint32_t frame = 0;
    };
    struct Note {
        std::vector<Movie> movies;
        bool held = false;
    };
    std::filesystem::path root;
    unsigned difficulty;
    int32_t tick = -151, backgroundStart = -150;
    bool backgroundChanged = false;
    OriginalAfpMoviePool pool;
    std::map<int32_t, Note> notes;
    std::map<std::string, std::unique_ptr<AfpPackage>> packages;
    std::map<std::pair<std::string, std::string>, std::unique_ptr<AfpTimeline>> timelines;
    std::map<std::pair<std::string, std::string>, AfpBitmap> bitmaps;
    std::set<std::string> unresolved;
    Impl(std::string path, unsigned d) : root(std::move(path)), difficulty(d) {
        if (d >= 4)
            throw Error("presentation difficulty outside original range");
    }
    AfpPackage &package(const std::string &name) {
        auto &p = packages[name];
        if (!p)
            p = std::make_unique<AfpPackage>(
                readFile((root / "data/graphics_rb6" / (name + ".ifs")).string()));
        return *p;
    }
    AfpTimeline &timeline(const std::string &pack, const std::string &name) {
        auto &p = timelines[{pack, name}];
        if (!p) {
            auto &archive = package(pack);
            p = std::make_unique<AfpTimeline>(archive.movieBytes(name),
                                              archive.bitStreamBytes(name));
        }
        return *p;
    }
    const AfpBitmap &bitmap(const std::string &pack, const std::string &name) {
        const auto key = std::make_pair(pack, name);
        auto found = bitmaps.find(key);
        if (found == bitmaps.end())
            found = bitmaps.emplace(key, package(pack).bitmap(name)).first;
        return found->second;
    }
    static uint32_t loopFrame(const AfpTimeline &movie, int64_t elapsed) {
        if (movie.root->frames.empty() || elapsed < 0)
            throw Error("invalid original AFP playback time");
        return uint32_t(uint64_t(elapsed) % movie.root->frames.size());
    }
    void control(Movie &m, bool held, bool head, bool forceLabel) {
        if (!m.poolSlot)
            return;
        auto &state = pool.state(unsigned(m.binding.poolIndex), *m.poolSlot);
        auto &tl = timeline(m.binding.package, m.binding.resource);
        if (head && (m.binding.resource == "swich_on" || m.binding.resource == "swich_off")) {
            m.frame = prepareSwitchMovieDraw(tl, m.binding.resource, state, tick, held).frame;
        } else if (!head && (m.binding.poolIndex == 27 || m.binding.poolIndex == 28 ||
                             m.binding.poolIndex == 29)) {
            const std::string label = held ? "on" : "off";
            if (forceLabel || state.label != label) {
                state.label = label;
                state.labelTick = tick;
            }
            state.visible = true;
            state.layerPlaying = true;
            m.frame = tl.resolveExportedLabelFrame(m.binding.resource, state.label,
                                                   uint32_t(tick - state.labelTick));
        } else if (!head &&
                   (m.binding.resource == "swich_on" || m.binding.resource == "swich_off")) {
            // Original intermediate caps are always displayed through the off path.
            m.frame = prepareSwitchMovieDraw(tl, m.binding.resource, state, tick, false).frame;
        } else {
            if (!state.layerPlaying) {
                state.layerPlaying = true;
                state.labelTick = tick;
            }
            state.visible = true;
            m.frame = loopFrame(tl, int64_t(tick) - state.labelTick);
        }
    }
    Movie *movieFor(int32_t id, int32_t slot, const std::string &resource) {
        auto found = notes.find(id);
        if (found == notes.end())
            throw Error("AFP note lifecycle missing");
        for (auto &m : found->second.movies)
            if (m.binding.slot == slot && m.binding.resource == resource)
                return &m;
        throw Error("original AFP binding absent: " + resource + " slot " + std::to_string(slot));
    }
    void sprite(PresentationFrame &out, std::string id, const std::string &pack,
                const std::string &name, float x, float y, int32_t layer, float angle, float sx,
                float sy, float alpha, float ax, float ay, uint32_t blend) {
        const auto &image = bitmap(pack, name);
        out.sprites.push_back({std::move(id),
                               pack,
                               name,
                               originalSpriteQuad(float(image.width), float(image.height), x, y,
                                                  angle, sx, sy, ax, ay),
                               {1, 1, 1, 1},
                               alpha,
                               layer,
                               blend, {}, std::nullopt});
    }
    void movie(PresentationFrame &out, std::string id, const std::string &pack,
               const std::string &name, uint32_t frame, float x, float y, int32_t layer,
               float angle, float sx, float sy, float alpha, float ax = .5f, float ay = .5f,
               const AfpSampleOptions &options = {}) {
        auto &tl = timeline(pack, name);
        const auto commands = tl.sampleExportedFrame(package(pack), name, frame, options);
        const float ox = tl.location[0] + float(tl.location[1] - tl.location[0]) * ax;
        const float oy = tl.location[2] + float(tl.location[3] - tl.location[2]) * ay;
        const float c = std::cos(angle), s = std::sin(angle);
        unsigned part = 0;
        for (const auto &draw : commands) {
            if (draw.blend != 0 && draw.blend != 8)
                throw Error("unsupported original AFP blend");
            std::array<MotionVector, 4> quad{{{draw.bounds[0], draw.bounds[3]},
                                              {draw.bounds[1], draw.bounds[3]},
                                              {draw.bounds[0], draw.bounds[2]},
                                              {draw.bounds[1], draw.bounds[2]}}};
            for (auto &p : quad) {
                const auto &m = draw.matrix;
                const float dx = (m.a * p.x + m.c * p.y + m.tx - ox) * sx;
                const float dy = (m.b * p.x + m.d * p.y + m.ty - oy) * sy;
                p = {x + c * dx - s * dy, y + s * dx + c * dy};
            }
            out.sprites.push_back({id + ":" + std::to_string(part++), pack, draw.texture, quad,
                                   draw.multiply, alpha, layer, draw.blend, {}, draw.solidColor});
            for (const auto &clip : draw.clips) {
                std::array<MotionVector, 4> transformed;
                for (unsigned i = 0; i < 4; ++i) {
                    const float dx = (clip[i][0] - ox) * sx, dy = (clip[i][1] - oy) * sy;
                    transformed[i] = {x + c * dx - s * dy, y + s * dx + c * dy};
                }
                out.sprites.back().clips.push_back(transformed);
            }
        }
    }
};
GamePresentation::GamePresentation(std::string root, unsigned difficulty)
    : impl(std::make_unique<Impl>(std::move(root), difficulty)) {}
GamePresentation::~GamePresentation() = default;
void GamePresentation::noteCreated(int32_t id, const std::vector<SessionMovieBinding> &bindings) {
    if (impl->notes.count(id))
        throw Error("duplicate original AFP note allocation");
    auto &note = impl->notes[id];
    for (const auto &binding : bindings) {
        auto slot = impl->pool.acquire(unsigned(binding.poolIndex));
        if (!slot)
            impl->unresolved.insert("original AFP pool exhausted: " +
                                    std::to_string(binding.poolIndex));
        note.movies.push_back({binding, slot, 0});
        if (slot &&
            (binding.poolIndex == 27 || binding.poolIndex == 28 || binding.poolIndex == 29)) {
            auto &state = impl->pool.state(unsigned(binding.poolIndex), *slot);
            state.label = "off";
            state.labelTick = int64_t(impl->tick) + 1;
        }
    }
}
void GamePresentation::noteReleased(int32_t id) {
    auto found = impl->notes.find(id);
    if (found == impl->notes.end())
        throw Error("original AFP release without acquisition");
    // 101591D0 resets long components before the head. 10156690 releases
    // complete body/cap/collection arrays in that order, then tail bg.
    // 1015BDE0 releases exp_loop slot0 before tail slot1; 10155890 releases
    // head on(-1) before off(-2). Creation order is different, and caps share
    // head pools 23/24, so preserve this order before pushing the LIFO list.
    const auto priority = [](const Impl::Movie &m) {
        return m.binding.slot < 0 ? 64 - m.binding.slot : m.binding.slot;
    };
    std::stable_sort(found->second.movies.begin(), found->second.movies.end(),
                     [&](const auto &a, const auto &b) { return priority(a) < priority(b); });
    for (const auto &movie : found->second.movies)
        if (movie.poolSlot)
            impl->pool.release(unsigned(movie.binding.poolIndex), *movie.poolSlot);
    impl->notes.erase(found);
}
void GamePresentation::advance(const SessionSnapshot &snapshot) {
    if (snapshot.tick != impl->tick + 1)
        throw Error("AFP requires each original 60 Hz tick");
    impl->tick = snapshot.tick;
    if (!impl->backgroundChanged && snapshot.players[0].finalRate() >= .69999999f) {
        impl->backgroundChanged = true;
        impl->backgroundStart = snapshot.tick;
    }
    for (auto &[id, note] : impl->notes) {
        (void)id;
        for (auto &m : note.movies)
            if (m.poolSlot)
                impl->pool.state(unsigned(m.binding.poolIndex), *m.poolSlot).visible = false;
    }
    for (const auto &note : snapshot.notes) {
        auto found = impl->notes.find(note.object);
        if (found == impl->notes.end())
            throw Error("original AFP actor was not created");
        auto &instance = found->second;
        const bool changed = instance.held != note.held;
        instance.held = note.held;
        // The held change affects all original body instances, including clipped ones.
        for (auto &m : instance.movies)
            if (m.binding.poolIndex == 27 || m.binding.poolIndex == 28 || m.binding.poolIndex == 29)
                impl->control(m, note.held, false, changed);
        if (!note.graphic.movie.empty()) {
            for (auto &m : instance.movies)
                if (m.binding.slot < 0 && m.binding.resource == note.graphic.movie)
                    impl->control(m, note.held, true, false);
        }
        for (const auto &d : note.longGraphic.draws)
            if (d.movie)
                impl->control(*impl->movieFor(note.object, d.slot, d.resource), note.held, false,
                              changed);
    }
}
PresentationFrame GamePresentation::frame(const SessionSnapshot &snapshot) {
    if (snapshot.tick != impl->tick)
        throw Error("presentation snapshot is not the prepared tick");
    PresentationFrame out;
    const auto background = impl->backgroundChanged ? "gmbg_01" : "gmbg_00";
    auto &tl = impl->timeline("gmbg_00", background);
    impl->movie(out, "background", "gmbg_00", background,
                Impl::loopFrame(tl, int64_t(snapshot.tick) - impl->backgroundStart), 384, 680, 79,
                0, 1, 1, 1);
    StageGraphicOptions stage;
    stage.topLaneCount = impl->difficulty >= 2 ? 3 : 2;
    unsigned part = 0;
    for (const auto &d : drawStageJudgmentGraphics(stage))
        impl->sprite(out, "stage:" + std::to_string(part++), "gmparts", d.name, float(d.x),
                     float(d.y), d.layer, d.angle, d.scale, d.scale, d.alpha, d.anchorX, d.anchorY,
                     d.afpBlend);
    const auto frameAge = uint32_t(snapshot.tick + 150);
    impl->movie(out, "frame", "gmframe_00", "gmframe", std::min(frameAge, 3u), -15, 0, 22,
                0, 1, 1, 1, 0, 0, stageFrameOptions(impl->difficulty, frameAge));
    out.unresolved.push_back("Original frame account text and portrait bindings");
    part = 0;
    for (const auto &d : drawStageLabels({}))
        impl->sprite(out, "stage-label:" + std::to_string(part++), "gmparts", d.name, float(d.x),
                     float(d.y), d.layer, d.angle, d.scale, d.scale, d.alpha, d.anchorX, d.anchorY,
                     d.afpBlend);
    StageScoreGraphicOptions score;
    score.score = snapshot.battle.displayedScore;
    score.combo = {snapshot.players[0].combo, snapshot.players[1].combo};
    score.comboYellow = {snapshot.players[0].counts[4] == 0, snapshot.players[1].fullCombo};
    part = 0;
    for (const auto &d : drawStageScoreGraphics(score))
        impl->sprite(out, "score:" + std::to_string(part++), "gmparts", d.name, float(d.x),
                     float(d.y), d.layer, d.angle, d.scale, d.scale, d.alpha, d.anchorX, d.anchorY,
                     d.afpBlend);
    for (const auto &note : snapshot.notes) {
        part = 0;
        const auto prefix = std::to_string(note.object) + ":";
        for (const auto &d : note.graphic.sprites)
            impl->sprite(out, prefix + "head:" + std::to_string(part++), "gmobject", d.name,
                         float(d.x), float(d.y), d.layer, d.angle, d.scale, d.scale, d.alpha,
                         d.anchorX, d.anchorY, d.afpBlend);
        if (!note.graphic.movie.empty()) {
            for (int slot : {-1, -2}) {
                auto found = impl->notes.find(note.object);
                if (found == impl->notes.end())
                    throw Error("AFP actor absent in frame");
                for (auto &m : found->second.movies)
                    if (m.binding.slot == slot && m.binding.resource == note.graphic.movie &&
                        m.poolSlot) {
                        const auto &d = note.graphic;
                        impl->movie(out, prefix + "head", m.binding.package, m.binding.resource,
                                    m.frame, float(d.movieX), float(d.movieY), d.movieLayer,
                                    d.movieAngle, d.movieScale, d.movieScale, d.movieAlpha);
                    }
            }
        }
        part = 0;
        for (const auto &d : note.longGraphic.draws) {
            const auto id = prefix + "long:" + std::to_string(part++);
            if (d.movie) {
                auto *m = impl->movieFor(note.object, d.slot, d.resource);
                if (m->poolSlot)
                    impl->movie(out, id, d.package, d.resource, m->frame, float(d.x), float(d.y),
                                d.layer, d.angle, d.scaleX, d.scaleY, d.alpha, d.anchorX,
                                d.anchorY);
            } else
                impl->sprite(out, id, d.package, d.resource, float(d.x), float(d.y), d.layer,
                             d.angle, d.scaleX, d.scaleY, d.alpha, d.anchorX, d.anchorY,
                             d.afpBlend);
        }
    }
    for (const auto &item : snapshot.unsupportedPresentation)
        if (item.rfind("AFP movie: ", 0) != 0)
            out.unresolved.push_back(item);
    out.unresolved.insert(out.unresolved.end(), impl->unresolved.begin(), impl->unresolved.end());
    return out;
}
std::vector<std::pair<std::string, std::string>> GamePresentation::prepareStageBitmaps() {
    std::vector<std::pair<std::string, std::string>> result;
    uint64_t bytes = 0;
    for (const auto &name : {"gmobject", "gmparts", "gmframe_00", "gmbg_00", "gmexp_effect"}) {
        const auto &textures = impl->package(name).textures();
        for (const auto &texture : textures) {
            const auto &r = texture.uvRect;
            bytes += uint64_t(r[1] - r[0]) * uint64_t(r[3] - r[2]) * 4;
            if (bytes > 128 * 1024 * 1024)
                throw Error("stage texture preparation exceeds memory budget");
            impl->bitmap(name, texture.nameCP932);
            result.emplace_back(name, texture.nameCP932);
        }
    }
    return result;
}
const AfpBitmap &GamePresentation::bitmap(const std::string &package, const std::string &texture) {
    return impl->bitmap(package, texture);
}
} // namespace rb
