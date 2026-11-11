#include "rb/AvsAlarms.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("AVS alarm regression"); }
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        rb::AvsDescriptors descriptors(64); uint32_t tick = 0;
        rb::AvsAlarms alarms(descriptors, 4, 50, [&] { return tick; });
        std::map<unsigned, uint32_t> tokens; std::vector<std::pair<unsigned,uint64_t>> calls;
        char op; unsigned id; uint64_t a, b;
        while (std::cin >> op >> id >> a >> b) {
            calls.clear(); int64_t result = 0;
            if (op == 'T') tick = uint32_t(a);
            else if (op == 'C') {
                auto callback = [&,id,b](uint64_t time) { calls.emplace_back(id,time); return b != 0; };
                if (a) result = alarms.createProcedure(10, callback);
                else { auto token = alarms.create(callback); result = token > 0; if (token > 0) tokens[id] = uint32_t(token); }
            } else if (op == 'S') result = b ? alarms.setOnce(tokens.at(id), a) : alarms.set(tokens.at(id), a);
            else if (op == 'D') result = alarms.destroy(tokens.at(id));
            else if (op == 'P') result = int64_t(alarms.poll());
            auto s = alarms.state();
            std::cout << result << ' ' << s.minimum << ' ' << s.used << ' ' << s.highWater;
            std::cout << ' ' << s.queue.size(); for (auto i : s.queue) std::cout << ' ' << i;
            std::cout << ' ' << s.free.size(); for (auto i = s.free.rbegin(); i != s.free.rend(); ++i) std::cout << ' ' << *i;
            for (auto r : s.records) std::cout << ' ' << r.deadline << ' ' << r.period << ' ' << r.allocated << ' ' << r.queued << ' ' << r.retired << ' ' << r.automatic;
            std::cout << ' ' << calls.size(); for (auto c : calls) std::cout << ' ' << c.first << ' ' << c.second;
            std::cout << '\n';
        } return 0;
    }
    rb::AvsDescriptors descriptors(16); uint32_t tick = 100;
    rb::AvsAlarms alarms(descriptors, 3, 50, [&] { return tick; });
    std::vector<unsigned> calls;
    const auto a = alarms.create([&](uint64_t time) { require(time == tick); calls.push_back(1); return true; });
    const auto b = alarms.create([&](uint64_t) { calls.push_back(2); return false; });
    require(a > 0 && b > 0 && alarms.create({}) == -2147090410);
    require(alarms.set(a, 10) == 0 && alarms.set(b, 10) == 0);
    require(alarms.setOnce(a, 999) == 1 && alarms.state().records[2].deadline == 110);
    tick = 110; require(alarms.poll() == 2 && calls == std::vector<unsigned>({1,2}));
    require(alarms.state().records[2].deadline == 120 && alarms.state().records[1].deadline == 0);
    require(alarms.set(a, 0) == 1 && alarms.set(a, 0) == 0);
    require(alarms.setOnce(a, 0) == -2147090410 && alarms.set(0, 2) == -2147090423);
    alarms.poll(); require(alarms.state().queue.empty());
    require(alarms.createProcedure(5, [&](uint64_t) { calls.push_back(3); return true; }));
    require(alarms.create([](uint64_t) { return true; }) == -2147090420);
    tick = 115; require(alarms.poll() == 1); alarms.poll(); require(alarms.state().used == 2);
    // A callback destroying another due alarm does not suppress its pending call.
    rb::AvsDescriptors d2(8); rb::AvsAlarms pending(d2, 2, 50, [&] { return tick; });
    int32_t second = 0;
    auto first = pending.create([&](uint64_t) { pending.destroy(second); return false; });
    second = pending.create([&](uint64_t) { calls.push_back(4); return false; });
    pending.set(first, 1); pending.set(second, 1); ++tick;
    require(pending.poll() == 2 && calls.back() == 4); pending.poll(); require(pending.state().used == 1);
    rb::AvsDescriptors d3(8); tick = 0xfffffff0;
    rb::AvsAlarms wrapping(d3, 1, 50, [&] { return tick; }); uint64_t observed = 0;
    auto wrap = wrapping.create([&](uint64_t now) { observed = now; return false; });
    wrapping.set(wrap, 32); tick = 16; require(wrapping.poll() == 1 && observed == UINT64_C(0x100000010));
    // This tests the native blocking boundary, not reserved AVS task admission.
    rb::AvsDescriptors d4(8); rb::AvsAlarms native(d4, 1, 50); std::atomic<unsigned> fired{0};
    auto timer = native.create([&](uint64_t) { ++fired; native.requestStop(); return false; });
    native.set(timer, 1); std::thread worker([&] { native.run(); }); worker.join();
    require(fired == 1 && !native.running());
    // Bulk descriptor cleanup retains stale payloads until descriptor reuse.
    // Reusing the alarm record must not leave a callback into its dead owner.
    rb::AvsDescriptors retained(8); uint32_t oldA, oldB, current, unrelated;
    unrelated = retained.allocate(5, nullptr, 0x1234);
    {
        rb::AvsAlarms owner(retained, 2, 50);
        oldA = uint32_t(owner.create([](uint64_t) { return true; }));
        oldB = uint32_t(owner.create([](uint64_t) { return true; }));
        retained.cleanupType(8);
        uintptr_t payload = 0;
        require(retained.lookup(oldA, &payload) && payload != 0);
        require(retained.lookup(oldB, &payload) && payload != 0);
        current = uint32_t(owner.create([](uint64_t) { return true; }));
        require(owner.state().used == 1);
        // One old token remains deferred; the other has been replaced by current.
        require(retained.lookup(oldA) != retained.lookup(oldB));
        require(unrelated != 0);
    }
    uintptr_t payload = 0xdead;
    require(retained.lookup(oldA) != retained.lookup(oldB));
    for (auto old : {oldA, oldB}) if (retained.lookup(old, &payload)) require(payload == 0);
    require(!retained.lookup(current));
    require(retained.lookup(unrelated, &payload) && payload == 0x1234);
    rb::AvsDescriptors deferred(4); uint32_t stale;
    {
        rb::AvsAlarms owner(deferred, 1, 50);
        stale = uint32_t(owner.create([](uint64_t) { return true; }));
        deferred.cleanupType(8);
    }
    require(deferred.lookup(stale, &payload) && payload == 0 && deferred.release(stale) == 0);
    rb::AvsDescriptors cappedDescriptors(2);
    rb::AvsAlarms capped(cappedDescriptors, 1, 3, [] { return 100u; });
    const auto cappedToken = capped.create([](uint64_t) { return true; });
    for (unsigned period = 20; period; --period) capped.set(cappedToken, period);
    require(capped.state().minimum == 101 && capped.state().pendingWakeSignals == 3);
    bool rejectedLimit = false;
    try { rb::AvsAlarms invalidLimit(cappedDescriptors, 1, 0); }
    catch (const std::invalid_argument &) { rejectedLimit = true; }
    require(rejectedLimit);
    std::cout << "PASS AVS alarms: finite pool, repeat/stop, callback order, deferred retirement, tick wrap and native dispatch\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }}
