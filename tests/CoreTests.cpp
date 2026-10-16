#include "rb/Assets.hpp"
#include "rb/Chart.hpp"
#include "rb/Judgment.hpp"
#include "rb/Network.hpp"
#include "rb/Viewport.hpp"
#include <cmath>
#include <functional>
#include <iostream>

using namespace rb;
static int checks = 0;
static void check(bool v) {
    ++checks;
    if (!v)
        throw Error("assertion " + std::to_string(checks));
}
static void rejects(const std::function<void()> &f) {
    try {
        f();
    } catch (const Error &) {
        ++checks;
        return;
    }
    throw Error("expected rejection");
}
static void le16(Bytes &b, uint16_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
static void le32(Bytes &b, uint32_t v) {
    le16(b, uint16_t(v));
    le16(b, uint16_t(v >> 16));
}
static Bytes fixture() {
    Bytes b{'R', 'B', 'F', 'F'};
    le32(b, 20);
    le32(b, 0);
    le32(b, 0);
    float bpm = 120;
    uint32_t raw;
    std::memcpy(&raw, &bpm, 4);
    le32(b, raw);
    le32(b, 10000);
    le32(b, 0);
    le16(b, 1);
    le16(b, 0);
    le16(b, 1);
    le16(b, 0);
    le32(b, 0);
    le32(b, 0);
    le32(b, uint32_t(-1000));
    le32(b, 1000);
    le16(b, 0);
    le16(b, 0);
    le16(b, 0);
    b.insert(b.end(), {0, 0, 0, 1});
    le16(b, 1000);
    le16(b, 2);
    le16(b, 0);
    le16(b, 0);
    le32(b, 8);
    le16(b, 0);
    le16(b, 0);
    le32(b, 0);
    le32(b, 0xffffffff);
    le32(b, 2);
    le32(b, 3);
    return b;
}
struct Mock : NetworkTransport {
    int calls = 0, status = 200;
    bool corrupt = false;
    std::function<void()> inFlight;
    NetworkResponse send(const NetworkRequest &q, const std::atomic_bool &) override {
        ++calls;
        if (inFlight)
            inFlight();
        return {status, corrupt ? Bytes{0} : q.body};
    }
};
struct LocalService : EmbeddedServer {
    int starts = 0, stops = 0, requests = 0;
    bool failStart = false;
    bool available() const override {
        return true;
    }
    void start() override {
        if (failStart)
            throw Error("synthetic startup failure");
        ++starts;
    }
    void stop() noexcept override {
        ++stops;
    }
    Bytes handle(const Bytes &input) override {
        ++requests;
        return input;
    }
};
int main() {
    try {
        auto original = fixture();
        auto c = Chart::parse(original);
        check(c.objects.size() == 1);
        check(c.objects[0].extension[0] == -1);
        check(c.additionalTicks(0) == 5);
        check(c.objects[0].destination() == 4);
        for (size_t i = 0; i < original.size(); ++i) {
            auto b = original;
            b.resize(i);
            rejects([&] { Chart::parse(b); });
        }
        auto extra = original;
        extra.push_back(0);
        rejects([&] { Chart::parse(extra); });
        check(millisecondsToTicks(-16) == 0);
        check(millisecondsToTicks(-17) == -1);
        check(millisecondsToTicks(1000) == 60);
        auto seed = randomSeed(1);
        check(seed[0] == 1812433253u);
        check(seed[1] == 2570827893u);
        check(classifyTiming(-2, 0, false, false) == TimingClass::inner);
        check(classifyTiming(3, 0, false, false) == TimingClass::inner);
        check(classifyTiming(4, 0, false, false) == TimingClass::middle);
        check(classifyTiming(-5, 0, false, false) == TimingClass::middle);
        check(classifyTiming(-6, 0, false, false) == TimingClass::outer);
        check(classifyTiming(-6, 0, false, true) == TimingClass::ignore);
        check(classifyTiming(2, 1, false, false) == TimingClass::middle);
        check(classifyTiming(2, 1, true, false) == TimingClass::inner);
        check(timingWindows(0xffffffff, false) == timingWindows(0, false));
        NoteTiming timing;
        timing.kind = 1;
        timing.durationTicks = 60;
        for (int tick = -65; tick <= 5; ++tick) {
            timing.relativeTick = tick;
            check(timing.isHoldScoringTick() ==
                  (tick == -15 || tick == -25 || tick == -35 || tick == -45));
        }
        timing.kind = 4;
        timing.segmentIndex = 0;
        timing.segmentDuration = 30;
        timing.segmentElapsed = 15;
        check(timing.isHoldScoringTick());
        timing.segmentElapsed = 16;
        check(!timing.isHoldScoringTick());
        timing.segmentElapsed = 30;
        check(timing.atSwitchBoundary());
        timing.segmentIndex = -1;
        timing.relativeTick = 1;
        check(!timing.atSwitchBoundary());
        timing.relativeTick = 0;
        check(timing.atSwitchBoundary());
        timing.relativeTick = -1000;
        check(!timing.expired());
        timing.relativeTick = -1001;
        check(timing.expired());
        timing.relativeTick = 0;
        timing.ended = true;
        check(timing.expired());
        for (double fraction : {.5, .75, 1.0})
            for (Rect safe : {Rect{0, 24, 1024, 1320}, Rect{20, 0, 1326, 1024}}) {
                auto v = ViewportTransform::fit(safe, fraction);
                for (Point p : {Point{0, 0}, Point{384, 680}, Point{767.99, 1359.99}}) {
                    auto q = v.toGame(v.toDisplay(p));
                    check(q.has_value());
                    check(std::abs(q->x - p.x) < 1e-8 && std::abs(q->y - p.y) < 1e-8);
                }
                check(!v.toGame({v.display.x - 1, v.display.y}));
            }
        rejects([] { ViewportTransform::fit({0, 0, 10, 10}, 0); });
        TouchState touch;
        touch.update(1, Point{2, 3});
        touch.update(2, Point{3, 4});
        check(touch.points().size() == 2);
        touch.update(1, {});
        check(touch.points().size() == 1);
        touch.reset();
        check(touch.points().empty());
        PropertyDocument doc;
        doc.root.name = "response";
        for (uint8_t type :
             {uint8_t(3), uint8_t(5), uint8_t(3), uint8_t(7), uint8_t(5), uint8_t(3)}) {
            Property p;
            p.name = "field";
            p.type = type;
            p.value = type == 3 ? Bytes{17} : type == 5 ? Bytes{1, 2} : Bytes{0, 0, 1, 3};
            doc.root.children.push_back(p);
        }
        Property str;
        str.name = "name";
        str.type = 11;
        str.value = {'t', 'e', 's', 't', 0};
        doc.root.children.push_back(str);
        doc.root.attributes["status"] = {'0', 0};
        auto packed = doc.encode();
        auto decoded = PropertyDocument::decode(packed);
        check(decoded.root.children.size() == 7);
        check(decoded.encode() == packed);
        check(decoded.root.children[0].integers()[0] == 17);
        check(decoded.root.children[1].integers()[0] == 258);
        for (size_t i = 0; i < packed.size(); ++i) {
            auto b = packed;
            b.resize(i);
            rejects([&] { PropertyDocument::decode(b); });
        }
        Bytes lz{7, 'A', 'B', 'C', 0, 0};
        check(decodeAvsLz(lz, 3) == Bytes({'A', 'B', 'C'}));
        rejects([&] { decodeAvsLz(lz, 2); });
        rejects([&] { decodeAvsLz(Bytes{0, 0x10, 0}, 3); });
        Bytes raw{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        check(decodeTextureBytes(raw, 12) == Bytes({9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 7, 8}));
        check(assetMD5({}) == "d41d8cd98f00b204e9800998ecf8427e");
        check(assetMD5(Bytes{'a', 'b', 'c'}) == "900150983cd24fb0d6963f7d28e17f72");
        check(decodeTextureBytes(Bytes{0, 0, 0, 4, 0, 0, 0, 0, 1, 2, 3, 4}, 4) ==
              Bytes({1, 2, 3, 4}));
        Wave wave;
        wave.format = 2;
        wave.channels = 2;
        wave.blockAlign = 15;
        wave.samplesPerBlock = 3;
        wave.frames = 3;
        wave.coefficients.push_back({256, 0});
        wave.encoded = {0, 0};
        le16(wave.encoded, 16);
        le16(wave.encoded, 16);
        le16(wave.encoded, 1000);
        le16(wave.encoded, uint16_t(-1000));
        le16(wave.encoded, 900);
        le16(wave.encoded, uint16_t(-900));
        wave.encoded.push_back(0x1f);
        check(wave.decodeBlock(0) == std::vector<int16_t>({900, -900, 1000, -1000, 1016, -1016}));
        wave.frames = 2;
        check(wave.decodeBlock(0).size() == 4);
        wave.encoded[0] = 7;
        rejects([&] { wave.decodeBlock(0); });
        auto mock = std::make_shared<Mock>();
        NetworkClient network(mock);
        std::atomic_bool cancelled{false};
        rejects([&] { network.exchange(doc, cancelled); });
        check(mock->calls == 0);
        network.configure({ServerMode::remote, "https://example.invalid/service", "remote-test"});
        network.beginSession();
        rejects([&] { network.configure({}); });
        network.endSession();
        check(network.exchange(doc, cancelled).root.name == "response");
        check(mock->calls == 1);
        cancelled = true;
        rejects([&] { network.exchange(doc, cancelled); });
        check(mock->calls == 1);
        cancelled = false;
        rejects([&] { network.exchange(doc, cancelled, std::chrono::milliseconds(0)); });
        mock->status = 500;
        rejects([&] { network.exchange(doc, cancelled); });
        mock->status = 200;
        mock->corrupt = true;
        rejects([&] { network.exchange(doc, cancelled); });
        rejects([&] { network.configure({ServerMode::embedded, "", "embedded"}); });
        check(network.config().mode == ServerMode::remote);
        mock->corrupt = false;
        mock->inFlight = [&] { cancelled = true; };
        rejects([&] { network.exchange(doc, cancelled); });
        cancelled = false;
        rejects([&] { network.configure({static_cast<ServerMode>(77), "", "invalid"}); });
        check(network.config().mode == ServerMode::remote);
        auto local = std::make_shared<LocalService>();
        {
            NetworkClient client({}, local);
            local->failStart = true;
            rejects([&] { client.configure({ServerMode::embedded, "", "local-test"}); });
            check(client.config().mode == ServerMode::offline);
            local->failStart = false;
            client.configure({ServerMode::embedded, "", "local-test"});
            check(local->starts == 1);
            check(client.exchange(doc, cancelled).root.name == "response");
            check(local->requests == 1);
            client.beginSession();
            rejects([&] {
                client.configure({ServerMode::remote, "https://example.invalid", "remote"});
            });
            check(local->stops == 0);
            client.endSession();
            client.configure({});
            check(local->stops == 1);
            rejects([&] { client.exchange(doc, cancelled); });
            check(local->requests == 1);
            client.configure({ServerMode::embedded, "", "local-test"});
        }
        check(local->starts == 2 && local->stops == 2);
        std::cout << "PASS " << checks << " assertions (no sockets)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
