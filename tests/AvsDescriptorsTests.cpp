#include "rb/AvsDescriptors.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
std::vector<uintptr_t> trace;
int32_t echo(uintptr_t value) { trace.push_back(value); return int32_t(value); }
rb::AvsDescriptors *reentrantPool;
uint32_t reentrantToken;
int32_t reenter(uintptr_t) { reentrantToken = reentrantPool->allocate(5, nullptr, 7); return 123; }
void require(bool ok) { if (!ok) throw std::runtime_error("AVS descriptor regression"); }
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        std::unique_ptr<rb::AvsDescriptors> pool;
        char op; uint32_t a, b, c;
        while (std::cin >> op >> a >> b >> c) {
            trace.clear();
            if (op == 'I') { pool = std::make_unique<rb::AvsDescriptors>(a); std::cout << a << '\n'; }
            else if (op == 'A') std::cout << pool->allocate(a, b ? echo : nullptr, c) << '\n';
            else if (op == 'R') std::cout << pool->release(a) << '\n';
            else if (op == 'S') std::cout << pool->replace(a, b ? echo : nullptr, c) << '\n';
            else if (op == 'L') { uintptr_t value = 0x12345678; auto ok = pool->lookup(a, &value); std::cout << ok << ' ' << value << '\n'; }
            else if (op == 'B') std::cout << pool->setGroup(a) << '\n';
            else if (op == 'C' || op == 'G') {
                if (op == 'C') pool->cleanupType(a); else pool->cleanupGroup(a);
                std::cout << trace.size(); for (auto value : trace) std::cout << ' ' << value; std::cout << '\n';
            }
        } return 0;
    }
    rb::AvsDescriptors pool(2);
    const auto first = pool.allocate(3, echo, 42);
    require(first == 0x860004);
    const auto second = pool.allocate(12, nullptr, 11);
    require(second == 0x1180005 && pool.allocate(1, nullptr, 0) == 0);
    require(pool.release(first) == 42 && !pool.lookup(first));
    require(pool.release(first) == 0 && !pool.replace(first, nullptr, 9));
    uintptr_t payload = 0;
    const uintptr_t wide = sizeof(uintptr_t) > 4 ? uintptr_t(UINT64_C(0x123456789abcdef0)) : 99;
    const auto third = pool.allocate(3, nullptr, wide);
    require((third & 0x1ffff) == 4 && third != first);
    require(pool.lookup(third, &payload) && payload == wide);
    require(pool.replace(third, echo, 55) && pool.release(third) == 55);
    require(pool.release(second) == 0);
    reentrantPool = &pool;
    auto token = pool.allocate(3, reenter, 0);
    require(pool.release(token) == 123 && (reentrantToken & 0x1ffff) == (token & 0x1ffff));
    require(pool.lookup(reentrantToken));
    rb::AvsDescriptors wrap(1);
    uint32_t firstToken = 0, repeated = 0;
    for (int i = 0; i < 257; ++i) {
        auto t = wrap.allocate(2, nullptr, 0); if (!i) firstToken = t;
        if (i == 256) repeated = t;
        wrap.release(t);
    }
    require(firstToken == repeated); // Original eight-bit generation aliases.
    rb::AvsDescriptors cleanup(8);
    require(cleanup.setGroup(2) == 0);
    const auto older = cleanup.allocate(5, echo, 10);
    const auto newer = cleanup.allocate(5, echo, 20);
    trace.clear(); cleanup.cleanupType(5);
    require(trace == std::vector<uintptr_t>({20, 10}));
    require(cleanup.lookup(newer) && cleanup.release(newer) == 0);
    require(cleanup.lookup(older));
    require((cleanup.allocate(7, nullptr, 0) & 0x1ffff) == (newer & 0x1ffff));
    rb::AvsDescriptors group(8); group.setGroup(1);
    group.allocate(8, echo, 8); group.allocate(4, echo, 4); group.allocate(1, echo, 1);
    trace.clear(); group.cleanupGroup(1);
    require(trace == std::vector<uintptr_t>({8, 4, 1}));
    std::cout << "PASS AVS descriptor allocation, release, stale handles, wrap and native payloads\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }}
