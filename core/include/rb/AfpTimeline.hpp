#pragma once
#include "AFP.hpp"
#include <memory>
#include <optional>
#include <set>

namespace rb {
// AP2 format interpretation references DragonMinded/bemaniutils (Unlicense),
// commit 1e66fcd780afb3088ce8e78c88615a143db492ee. See docs/AFP_TIMELINE.md.
struct AfpMatrix {
    float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
};
using AfpClipQuad = std::array<std::array<float, 2>, 4>;
struct AfpSampleOptions {
    std::set<std::string> hiddenObjects;
    std::map<std::string, std::string> textureOverrides;
    std::optional<uint32_t> playbackTicks;
};
struct AfpPlacement {
    uint32_t flags = 0;
    uint16_t object = 0, depth = 0, source = 0;
    AfpMatrix matrix;
    std::string instanceName; // original named movie object, no external override applied
    float originX = 0, originY = 0;
    std::array<float, 4> multiply{1, 1, 1, 1}, addition{};
    uint8_t blend = 0;
    int32_t reportedFrame = -1; // aeplib.aep_set_set_frame callback metadata
    std::optional<std::array<float, 4>> rectangularMask;
};
struct AfpTag {
    uint16_t kind = 0, id = 0;
    size_t offset = 0;
    Bytes payload;
    struct Timeline;
    std::shared_ptr<Timeline> sprite;
};
struct AfpTag::Timeline {
    struct Frame {
        uint32_t first = 0, count = 0;
    };
    std::vector<AfpTag> tags;
    std::vector<Frame> frames;
    std::map<std::string, uint16_t> labels;
};
struct AfpSpriteCommand {
    std::string texture;
    AfpMatrix matrix;              // composed placement transform, including rotation origin
    std::array<float, 4> bounds{}; // left/right/top/bottom
    std::array<float, 4> multiply{1, 1, 1, 1}, addition{};
    uint8_t blend = 0;
    std::vector<AfpClipQuad> clips;
    std::string instancePath;
};
class AfpTimeline {
    Bytes decoded_;
    size_t strings_ = 0, stringSize_ = 0;
    std::map<uint16_t, const AfpTag *> definitions_;
    std::shared_ptr<AfpTag::Timeline> parseTimeline(size_t, unsigned depth);
    std::string stringAt(uint16_t) const;
    void index(const AfpTag::Timeline &);
    struct ScriptEffect {
        bool stop = false;
        std::string go;
        int32_t reportedFrame = -1;
        std::optional<std::array<float, 4>> rectangularMask;
    };
    ScriptEffect script(const Bytes &) const;

  public:
    std::string name;
    float fps = 0;
    std::array<uint16_t, 4> location{};
    std::map<std::string, uint16_t> exports;
    std::shared_ptr<AfpTag::Timeline> root;
    AfpTimeline(Bytes movie, const Bytes &bsi);
    AfpPlacement placement(const AfpTag &) const;
    // Deterministic sampling of an exported symbol's display-list frames.
    // This deliberately does not execute movie-global imports/initializers.
    // Unknown scripts, masks, non-rect geometry and unknown options throw Error.
    std::vector<AfpSpriteCommand> sampleExportedFrame(const AfpPackage &, const std::string &symbol,
                                                      uint32_t frame,
                                                      const AfpSampleOptions &options = {}) const;
    uint32_t resolveExportedLabelFrame(const std::string &symbol, const std::string &label,
                                       uint32_t elapsed) const;
    // Label API frame numbers are 1-based. Executes supported stop/goto actions.
    std::vector<AfpSpriteCommand> sampleExportedLabelFrame(const AfpPackage &,
                                                           const std::string &symbol,
                                                           const std::string &label,
                                                           uint32_t elapsed) const;
};
// One original pooled movie instance. Keep this state through visibility changes
// and pool reuse; CNoteGraphic acquisition does not seek/reset an existing movie.
struct SwitchMovieState {
    std::string label; // empty: newly created original movie at frame0, rate0
    int64_t labelTick = 0;
    bool layerPlaying = false, visible = false;
};
struct SwitchMovieSample {
    std::string label;
    uint32_t elapsed = 0, frame = 0; // zero-based display-list frame
};
SwitchMovieSample prepareSwitchMovieDraw(const AfpTimeline &, const std::string &symbol,
                                         SwitchMovieState &, int64_t tick,
                                         bool held); // original 10155A00 movie control
} // namespace rb
