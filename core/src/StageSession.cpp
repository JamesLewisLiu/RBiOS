#include "rb/StageSession.hpp"
#include "rb/ScoreDisplay.hpp"
#include "rb/NoteFrame.hpp"
#include "rb/Route.hpp"
#include "rb/StageOrder.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
namespace rb {
namespace {
int launchX(int lane, int wide) {
    switch (lane) {
    case 0:
        return wide == 1 ? 32 : 96;
    case 1:
        return wide == 1 ? 128 : 192;
    case 2:
        return wide == 1 ? 224 : 288;
    case 3:
        return 384;
    case 4:
        return wide == 1 ? 544 : 480;
    case 5:
        return wide == 1 ? 640 : 576;
    case 6:
        return wide == 1 ? 736 : 672;
    default:
        return 0;
    }
}
int pathKind(int reason, const SheetObject &o) {
    int result = reason == 0 || reason == 6 ? 1
                 : reason == 1              ? 5
                 : reason == 10             ? 2
                 : reason == 11             ? 6
                 : reason == 12             ? 7
                                            : 4;
    return result == 7 && o.attributes[0] > 0 ? 6 : result;
}
} // namespace
struct StageSession::Impl {
    struct Slot {
        size_t object = 0;
        RuntimeNote note;
        NoteFrame frame;
        MotionPosition position;
        MotionTimeline motion;
        BuiltRoute route;
        RouteInput routeInput;
        ChartRouteConfig config;
        NoteGraphic graphic;
        LongNoteGraphicStyle longStyle;
        LongNoteGraphicState longState;
        SwitchLongGraphicState switchState;
        LongGraphicDraws longDraws;
        std::vector<SessionMovieBinding> movieBindings;
        int layer = 40;
        int32_t graphicLabelTick = 0;
        bool graphicHeld = false;
    };
    MusicSheet sheet;
    SessionOptions options;
    std::vector<std::unique_ptr<Slot>> pool;
    std::vector<Slot *> order;
    std::vector<bool> created;
    std::array<PlayerScore, 2> players;
    std::array<StageScoreState, 2> scoring;
    BattleScore battle;
    CPUState cpu;
    OriginalRandom random;
    TouchInput touch;
    int32_t tick = -151, createdCount = 0;
    size_t initialCursor = 0;
    bool dirty = true, complete = false;
    std::string failure;
    Impl(MusicSheet s, SessionOptions o)
        : sheet(std::move(s)), options(std::move(o)), created(sheet.objects.size()),
          battle(options.initialBattle), random(OriginalRandom::seeded(options.randomSeed)) {
        if (options.endTick <= 0)
            throw Error("invalid original minimum completion tick");
        for (unsigned side = 0; side < 2; ++side) {
            unsigned chartSide = side == 0 ? unsigned(sheet.options.localChartSide)
                                           : unsigned(1 - sheet.options.localChartSide);
            auto &p = players[side];
            p.side = int(side);
            p.chartSide = int(chartSide);
            p.totalEvents = sheet.counts.total[chartSide];
            p.extraEvents = sheet.counts.longTicks[chartSide];
            p.totalJustReflec = sheet.counts.justReflec[chartSide];
            p.reset();
            scoring[side].difficulty = unsigned(sheet.options.difficulty);
        }
        cpu.initialize(unsigned(sheet.counts.objects[unsigned(1 - sheet.options.localChartSide)]),
                       options.cpuDifficulty, random);
        for (unsigned i = 0; i < 256; ++i) {
            pool.push_back(std::make_unique<Slot>());
            order.push_back(pool.back().get());
        }
    }
    Slot *lookup(int16_t id) {
        for (auto *s : order)
            if (s->frame.active && sheet.objects[s->object].source.id == id)
                return s;
        return nullptr;
    }
    void rebuild(Slot &s) {
        const auto &o = sheet.objects[s.object];
        s.routeInput.side = o.oppositeSide;
        s.routeInput.kind = o.attributes[3];
        s.routeInput.widthMode = o.widthMode;
        s.routeInput.suppressWallReflection = o.attributes[2] == 1;
        s.route = buildRoute(s.routeInput, options.input.field);
        s.motion = MotionTimeline::initialize(s.route.route, s.note.judgment.relativeTick,
                                              s.route.route.intermediateCount);
        s.position.delta = s.motion.velocity;
    }
    void initializePresentation(Slot &s, const NoteGraphicInput &graphic) {
        auto &bindings = s.movieBindings;
        const auto bind = [&](int slot, int pool, std::string resource,
                              std::string package = "gmobject") {
            bindings.push_back({slot, pool, std::move(resource), std::move(package)});
        };
        // 10155F10 allocates both switch-head movies, even while hidden.
        if (s.graphic.style == 7 || s.graphic.style == 8) {
            bind(-1, graphic.side ? 25 : 23, "swich_on");
            bind(-2, graphic.side ? 26 : 24, "swich_off");
        } else if (!s.graphic.movieOn.empty()) {
            bind(-1, s.graphic.style == 5 ? 31 : 32, s.graphic.movieOn);
        }
        // 10158F20 only creates local long/switch body components.
        if (s.note.side)
            return;
        if (s.note.judgment.kind == 1) {
            s.longStyle = selectLongNoteGraphic(graphic.color, graphic.top, graphic.side,
                                                graphic.presentationFlags);
            s.longState.active = true;
            s.longState.bodyVisible = false; // 1015BDE0; 1015A5E0 enables it later
            bind(1, graphic.side ? 16 : 15, s.longStyle.tailEffect); // 1015C1A0 allocation order
            bind(0, 30, "exp_loop", "gmexp_effect");
        } else if (s.note.judgment.kind == 4) {
            s.switchState.active = true;
            s.switchState.color = graphic.color;
            s.switchState.clipY = options.input.field.y(4); // default global12477E34 = 1098
            const auto &object = sheet.objects[s.object];
            int64_t previous = int64_t(object.start) + object.travel;
            const auto count = std::min(size_t(16), object.extraIndices.size());
            for (size_t i = 0; i < count; ++i) {
                const auto &extra = sheet.chart.extras.at(object.extraIndices[i]);
                // 101569E0 uses the sum of separately truncated extra tick fields.
                const int64_t end = int64_t(millisecondsToTicks(extra.timeMs)) +
                                    millisecondsToTicks(extra.durationMs);
                const int64_t duration = end - previous;
                if (duration < INT32_MIN || duration > INT32_MAX)
                    throw Error("original switch graphic duration overflow");
                SwitchGraphicSegment segment;
                segment.duration = int32_t(duration);
                segment.special = (extra.value & 1) != 0;
                segment.secondarySpecial = (extra.value & 2) != 0;
                // 10156950 collection belongs to replay association; current session is offline.
                s.switchState.segments.push_back(segment);
                bind(int(i), i % 2 ? 28 : 27, i % 2 ? "swich_long_body_off" : "swich_long_body_on");
                bind(16 + int(i), i % 2 ? 23 : 24, i % 2 ? "swich_on" : "swich_off");
                bind(32 + int(i), 33, "eff_justcollection");
                previous = end;
            }
            bind(48, 29, "long_tail_bg_switch");
        }
    }
    void updatePresentation(Slot &s) {
        if (s.longState.active) {
            const auto &f = s.frame;
            if (f.holdTrail) {
                s.longState.bodyVisible = true;
                updateLongNoteGraphic(s.longState, {f.x, f.y}, {f.endpointX, f.endpointY}, 1);
            } else
                updateLongNoteGraphicDirection(s.longState, {f.x, f.y}, s.position.delta);
            s.longState.tailScale = f.scale;
            s.longState.alpha = f.opacity;
            s.longDraws = drawLongNoteGraphic(s.longStyle, s.longState);
        } else if (s.switchState.active) {
            const auto &f = s.frame;
            auto &g = s.switchState;
            g.head = {f.x, f.y};
            g.trail = {f.trailX, f.trailY};
            g.velocity = s.position.delta;
            g.trailTicks = f.trailTicks;
            g.secondaryTrailTicks = f.secondaryTrailTicks;
            g.scale = f.scale;
            g.alpha = f.opacity;
            g.held = s.note.judgment.held;
            updateSwitchLongGraphic(g);
            s.longDraws = drawSwitchLongGraphic(g);
        }
    }
    void presentationJudgment(Slot &s, const JudgmentResult &r) {
        if (!s.longState.active)
            return;
        // Original 10159E10 invokes BF30/BEE0 from the unnormalized reason.
        if ((r.reason == 3 && r.grade != 4) || r.reason == 7)
            s.longState.headEffect = s.longState.tailEffect = true;
        else if (r.reason == 8)
            s.longState.headEffect = s.longState.tailEffect = false;
    }
    Slot &create(size_t index, ChartRouteConfig config) {
        if (index >= sheet.objects.size())
            throw Error("stage link outside original sheet");
        Slot *slot = nullptr;
        for (auto *s : order)
            if (!s->frame.active) {
                slot = s;
                break;
            }
        if (!slot)
            throw Error("original 256-note pool exhausted");
        *slot = Slot{};
        auto &s = *slot;
        auto &n = s.note;
        auto &f = s.frame;
        const auto &o = sheet.objects[index];
        s.object = index;
        s.config = config;
        s.graphicLabelTick = tick;
        n.chartIndex = o.index;
        n.side = o.side;
        n.flags = o.source.flags;
        n.judgment.kind = o.attributes[3];
        n.judgment.style = 6;
        n.judgment.switchDirection = -1;
        n.judgment.contactId = config.specialCounter;
        n.judgment.switchCount = int(o.extraIndices.size());
        n.switchDurations = sheet.chart.switchDurations(index);
        n.touch.phase = 9999;
        n.touch.region = o.attributes[2];
        n.touch.side = o.side;
        n.touch.wide = o.wide;
        n.touch.widthMode = o.widthMode;
        n.touch.initialTiming = o.source.parameters[0];
        n.touch.fieldBottom = options.input.field.bottom;
        auto normal = timingWindows(options.input.timingMode, false),
             alt = timingWindows(options.input.timingMode, true);
        n.touch.windowStart = normal[7];
        n.touch.windowEnd = normal[0];
        n.touch.switchWindowStart = alt[7];
        int selector = o.attributes[2] == 1 ? (o.side ? 7 : 6) : (o.side ? 5 : 4);
        int line = options.input.field.y(unsigned(selector));
        n.touch.boundaryY = line - 108;
        n.touch.extendedBottom = line + 108;
        NoteInitialization init;
        init.tick = tick;
        init.chartStartTick = o.start;
        init.chartTravelTicks = o.travel;
        init.pathMode = config.side;
        init.pathDelay = config.pathKind;
        init.durationTicks =
            (o.attributes[3] == 1 || o.attributes[3] == 4) ? o.longDuration(sheet.chart) : 0;
        init.previousId = (o.source.flags & 8) ? int16_t(o.previous) : -1;
        initializeNote(n, f, init, [&](int16_t id) -> RuntimeNote * {
            auto *p = lookup(id);
            return p ? &p->note : nullptr;
        });
        f.x = config.start.x;
        f.y = config.start.y;
        s.position.rounded = config.start;
        s.position.position = {float(f.x), float(f.y)};
        if ((o.source.flags & 8) && o.previous != -1) {
            auto *previous = lookup(int16_t(o.previous));
            if (!previous)
                throw Error("original linked note has no active predecessor");
            s.config = previous->config;
            s.routeInput = previous->routeInput;
            s.route = previous->route;
            auto point = (n.flags & 0x10) ? s.config.start
                                          : MotionPoint{previous->frame.x, previous->frame.y};
            f.x = point.x;
            f.y = point.y;
            s.position.rounded = point;
            s.position.position = {float(point.x), float(point.y)};
            s.motion = MotionTimeline::initialize(s.route.route, n.judgment.relativeTick,
                                                  s.route.route.intermediateCount);
            s.position.delta = s.motion.velocity;
            f.copiedPath = true;
        } else {
            RouteSeedConfig seed;
            seed.start = config.start;
            seed.pathClass = config.pathClass;
            seed.side = config.side;
            seed.destination = config.destination;
            seed.direction = config.direction;
            s.routeInput = initializeRoute(seed, options.input.field);
            rebuild(s);
        }
        f.pathReady = s.config.pathKind == 6 || s.config.pathKind == 4;
        f.active = true;
        selectInitialState(n, f);
        if (n.flags & 0x10) {
            f.scale = .69999999f;
            f.opacity = 0;
        }
        NoteGraphicInput graphic;
        graphic.color = o.attributes[1];
        graphic.top = o.attributes[2];
        graphic.kind = o.attributes[3];
        graphic.side = o.side;
        graphic.special = int(o.source.links.size());
        graphic.destination = uint32_t(o.destination);
        graphic.routeFlags = uint32_t(o.directionFlags);
        graphic.widthMode = o.widthMode;
        graphic.flags = o.source.flags;
        s.graphic = selectNoteGraphic(graphic);
        initializePresentation(s, graphic);
        n.touch.x = f.x;
        n.touch.y = f.y;
        created[index] = true;
        ++createdCount;
        dirty = true;
        if (options.onNoteCreated)
            options.onNoteCreated(o.source.id, s.movieBindings);
        return s;
    }
    void launchSources() {
        while (initialCursor < sheet.initialObjects.size()) {
            auto index = sheet.initialObjects[initialCursor];
            const auto &o = sheet.objects[index];
            if (o.start < tick)
                throw Error("original source note launch tick was skipped");
            if (o.start > tick)
                break;
            ChartRouteConfig c;
            c.start = {launchX(o.launchLane, o.widthMode),
                       options.input.field.y(o.oppositeSide ? 3 : 2)};
            c.destination = o.routeDestination;
            c.side = o.side;
            c.noteVariant = o.attributes[2];
            c.parameter = 6;
            c.direction = -1;
            create(index, c);
            ++initialCursor;
        }
    }
    void launchLinks(Slot &parent, const SubmittedJudgment &r) {
        if (!r.scored)
            return;
        const auto links = sheet.objects[parent.object].source.links;
        for (auto link : links) {
            if (link == -1)
                break;
            if (link < 0 || size_t(link) >= created.size())
                throw Error("original stage linked target invalid");
            if (created[size_t(link)])
                continue;
            const auto &o = sheet.objects[size_t(link)];
            ChartRouteInput in;
            in.noteVariant = o.attributes[2];
            in.side = o.side;
            in.destination = o.routeDestination;
            in.direction = o.routeDirection;
            in.pathKind = pathKind(r.normalized.reason, o);
            in.parameter = r.original.grade;
            in.specialCounter = touch.state.lastSelectedId;
            in.previousTarget = parent.route.route.points[4];
            create(size_t(link), configureChartRoute(in));
        }
    }
    void specialRoute(Slot &s) {
        s.config.pathClass = 2 - (s.config.noteVariant != 0);
        s.config.overrideKind = s.config.pathKind == 7 || s.config.pathKind == 8 ? 0
                                : s.config.pathKind == 9                         ? 1
                                                                                 : -1;
        redirectRoute(s.routeInput, s.config, {s.frame.x, s.frame.y}, options.input.field);
        rebuild(s);
        s.position.suppressOnce = true;
        s.frame.pathReady = true;
        s.note.special = true;
        const auto &o = sheet.objects[s.object];
        players[unsigned(o.oppositeSide)].consumeJustReflec();
        // Original propagates copied route to the active linked successors.
        for (auto *n = s.note.next; n; n = n->next) {
            Slot *linked = nullptr;
            for (auto *candidate : order)
                if (&candidate->note == n) {
                    linked = candidate;
                    break;
                }
            if (!linked)
                throw Error("active linked route missing from pool");
            linked->config = s.config;
            linked->routeInput = s.routeInput;
            linked->route = s.route;
            linked->motion = MotionTimeline::initialize(s.route.route, n->judgment.relativeTick,
                                                        s.route.route.intermediateCount);
            linked->frame.x = s.frame.x;
            linked->frame.y = s.frame.y;
            linked->position.position = s.position.position;
            linked->position.rounded = s.position.rounded;
            linked->position.delta = linked->motion.velocity;
            linked->frame.copiedPath = true;
            linked->frame.pathReady = true;
        }
    }
    void sortNotes() {
        std::vector<StageOrderEntry> entries;
        for (size_t i = 0; i < order.size(); ++i)
            entries.push_back(
                {uint32_t(i), order[i]->frame.active, order[i]->note.judgment.relativeTick});
        sortStageOrder(entries);
        auto before = order;
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = before[entries[i].identity];
        std::array<std::array<int, 2>, 2> counter{{{{0, 3}}, {{0, 3}}}};
        for (auto *s : order)
            if (s->frame.active) {
                auto &count = counter[s->note.touch.region == 1 ? 1 : 0][unsigned(s->note.side)];
                s->layer = std::clamp(count + (s->note.touch.region != 1) + 39, 39, 60);
                ++count;
            }
        dirty = false;
    }
    float bpmAt(int32_t at) const {
        float bpm = sheet.chart.bpm;
        // 0x1014A230 imports command type 3, wire offset +16, into the
        // ordered tempo list queried by 0x10148E80.
        for (const auto &command : sheet.chart.commands)
            if (command.type == 3) {
                if (millisecondsToTicks(command.timeMs) > at)
                    break;
                static_assert(sizeof(bpm) == sizeof(command.fields[2]));
                std::memcpy(&bpm, &command.fields[2], sizeof(bpm));
                if (!std::isfinite(bpm) || bpm <= 0)
                    throw Error("invalid original tempo command");
            }
        return bpm;
    }
    void step(const std::vector<TouchPoint> &points) {
        ++tick;
        touch.advance(points, {70, 80, 1142});
        if (dirty)
            sortNotes();
        // 0x1015FC60: actor state update precedes judgments and source launches;
        // 0x101577D0/1015AC00 are the separate frame-end pass.
        for (auto *s : order)
            if (s->frame.active) {
                const auto &o = sheet.objects[s->object];
                NoteFrameContext c;
                c.tick = tick;
                c.chartStartTick = o.start;
                c.bpm = bpmAt(o.targetTick());
                c.field = options.input.field;
                c.path = {s->config.pathKind, int(s->route.route.intermediateCount),
                          s->config.overrideKind};
                c.acceptRemoteRoute = [] { return false; }; // explicit offline: no remote session
                c.specialRouteAvailable = [&] {
                    return s->config.parameter == 0 && o.attributes[0] == 0 &&
                           s->config.pathKind >= 7 && s->config.pathKind <= 9;
                };
                c.buildSpecialRoute = [&] {
                    specialRoute(*s);
                    c.path = {s->config.pathKind, int(s->route.route.intermediateCount),
                              s->config.overrideKind};
                };
                c.initializeTopTimeline = [&] {
                    s->motion =
                        MotionTimeline::initialize(s->route.route, s->note.judgment.relativeTick,
                                                   s->route.route.intermediateCount);
                };
                advanceNoteState(s->note, s->frame, s->position, s->motion, c);
            }
        for (size_t i = 0; i < order.size(); ++i) {
            auto *s = order[i];
            if (!s->frame.active)
                break;
            auto rules = options.input;
            rules.mode = options.inputModes[unsigned(s->note.side)];
            auto &score = scoring[unsigned(s->note.side)];
            auto result =
                evaluateNote(s->note, touch.state, rules, battle, players[unsigned(s->note.side)],
                             score, s->note.side && rules.mode == 3 ? &cpu : nullptr, &random);
            if (result) {
                presentationJudgment(*s, result->original);
                if (options.onJudgment)
                    options.onJudgment({tick, int32_t(s->object), s->note.side,
                                        s->note.judgment.kind, s->note.judgment.relativeTick,
                                        *result});
                launchLinks(*s, *result);
                dirty = dirty || score.dirty;
                score.dirty = false;
            }
            if (s->graphicHeld != s->note.judgment.held) {
                s->graphicHeld = s->note.judgment.held;
                s->graphicLabelTick = tick;
            }
        }
        launchSources();
        for (auto *s : order)
            if (s->frame.active) {
                // 1015AC00 exports current geometry before motion/frame counters advance.
                updatePresentation(*s);
                if (s->frame.phase != NotePhase::exit)
                    s->motion.advance();
                const bool expired = finishNoteFrame(s->note, s->frame);
                // 1015AC00 chooses swich_on/off by original note+372.
                if (s->note.judgment.kind == 4) {
                    if (s->note.judgment.switchDirection == 0)
                        s->graphic.style = 7;
                    else if (s->note.judgment.switchDirection == 1)
                        s->graphic.style = 8;
                }
                if (expired) {
                    if (options.onNoteReleased)
                        options.onNoteReleased(sheet.objects[s->object].source.id);
                    if (s->note.previous)
                        s->note.previous->next = nullptr;
                    if (s->note.next) {
                        s->note.next->previous = nullptr;
                        s->note.next->touch.inputBlocked = true;
                    }
                    s->note.previous = s->note.next = nullptr;
                    s->frame.active = false;
                    dirty = true;
                }
            }
        bool any = false;
        for (auto *s : order)
            any = any || s->frame.active;
        complete =
            tick >= options.endTick && ((createdCount >= int(sheet.objects.size()) && !any) ||
                                        tick >= millisecondsToTicks(sheet.chart.durationMs) + 660);
    }
};
StageSession::StageSession(MusicSheet s, SessionOptions o)
    : impl(std::make_unique<Impl>(std::move(s), std::move(o))) {}
StageSession::~StageSession() = default;
StageSession::StageSession(StageSession &&) noexcept = default;
StageSession &StageSession::operator=(StageSession &&) noexcept = default;
void StageSession::advanceToTick(int32_t target, const std::vector<TouchPoint> &points) {
    if (!impl->failure.empty())
        throw Error(impl->failure);
    if (target < impl->tick)
        throw Error("original stage clock cannot move backwards");
    try {
        while (impl->tick < target && !impl->complete) {
            impl->step(points);
            updateScoreDisplay(impl->battle); // Original manager update follows actor updates.
            if (impl->options.onPresentationFrame)
                impl->options.onPresentationFrame(snapshot());
        }
    } catch (const std::exception &e) {
        std::ostringstream message;
        message << "tick " << impl->tick << ": " << e.what();
        impl->failure = message.str();
        throw Error(impl->failure);
    }
}
void StageSession::cancelTouches() {
    impl->touch.reset();
}
SessionSnapshot StageSession::snapshot() const {
    SessionSnapshot out;
    out.tick = impl->tick;
    out.complete = impl->complete;
    out.created = impl->createdCount;
    out.players = impl->players;
    out.battle = impl->battle;
    out.failure = impl->failure;
    out.processed = impl->scoring[0].processed + impl->scoring[1].processed;
    for (auto *s : impl->order)
        if (s->frame.active) {
            SessionNoteView v;
            v.object = impl->sheet.objects[s->object].source.id;
            v.side = s->note.side;
            v.kind = s->note.judgment.kind;
            v.x = s->frame.x;
            v.y = s->frame.y;
            v.relativeTick = s->note.judgment.relativeTick;
            v.createdTick = s->frame.initializedAt;
            v.movieLabelTick = s->graphicLabelTick;
            v.lifetimeTicks = impl->tick - v.createdTick;
            v.scale = s->frame.scale;
            v.opacity = s->frame.opacity;
            v.held = s->note.judgment.held;
            v.special = s->note.special;
            v.longGraphic = s->longDraws;
            v.movieBindings = s->movieBindings;
            const float direction = float(double(float(std::atan2(-1.0, 0.0))) -
                                          double(float(std::atan2(-double(s->position.delta.y),
                                                                  double(s->position.delta.x)))));
            v.direction = direction;
            v.graphic = drawNoteGraphic(s->graphic, v.x, v.y, direction, v.scale, v.opacity, 1,
                                        s->layer, v.held);
            if (!v.graphic.movie.empty())
                out.unsupportedPresentation.push_back("AFP movie: " + v.graphic.movie);
            if (v.graphic.needsSpecialOverlay || v.special)
                out.unsupportedPresentation.push_back("original JR overlay");
            if (!v.longGraphic.overlays.empty())
                out.unsupportedPresentation.push_back("original switch JR overlay queue");
            out.notes.push_back(std::move(v));
        }
    std::sort(out.unsupportedPresentation.begin(), out.unsupportedPresentation.end());
    out.unsupportedPresentation.erase(
        std::unique(out.unsupportedPresentation.begin(), out.unsupportedPresentation.end()),
        out.unsupportedPresentation.end());
    return out;
}
} // namespace rb
