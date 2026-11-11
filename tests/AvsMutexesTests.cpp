#include "rb/AvsMutexes.hpp"
#include "rb/AvsResourcePool.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

namespace { void require(bool value) { if (!value) throw std::runtime_error("AVS mutex regression"); } }
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        std::unique_ptr<rb::AvsDescriptors> descriptors;
        std::unique_ptr<rb::AvsMutexes> mutexes;
        char op; uint32_t a, b;
        while (std::cin >> op >> a >> b) {
            if (op == 'I') {
                mutexes.reset(); descriptors = std::make_unique<rb::AvsDescriptors>(a);
                mutexes = std::make_unique<rb::AvsMutexes>(*descriptors, uint16_t(b));
                std::cout << mutexes->available() << '\n';
            } else if (op == 'C') { auto token = mutexes->create(); std::cout << token << ' ' << mutexes->available() << '\n'; }
            else if (op == 'D') { auto result = mutexes->destroy(a); std::cout << result << ' ' << mutexes->available() << '\n'; }
        } return 0;
    }
    rb::AvsDescriptors descriptors(8);
    {
        rb::AvsMutexes mutexes(descriptors, 2);
        mutexes.registerCurrentThread(1);
        auto service = mutexes.startDescriptorService();
        require(service == 0x860004 && mutexes.available() == 1);
        auto other = mutexes.create();
        require(other == 0x1060005 && mutexes.create() == 0);
        bool rejected = false;
        try { mutexes.lock(other, 0); } catch (const std::logic_error &) { rejected = true; }
        require(rejected);
        mutexes.lock(other, 1); mutexes.lock(other, 1);
        rejected = false;
        try { mutexes.destroy(other); } catch (const std::logic_error &) { rejected = true; }
        require(rejected && descriptors.lookup(other));
        std::promise<void> attempting;
        std::atomic<bool> entered{false};
        auto waiter = std::async(std::launch::async, [&] {
            mutexes.registerCurrentThread(2);
            attempting.set_value();
            mutexes.lock(other, 2); entered = true; mutexes.unlock(other); mutexes.unregisterCurrentThread();
        });
        attempting.get_future().wait();
        require(!entered.load());
        mutexes.unlock(other);
        require(!entered.load());
        mutexes.unlock(other);
        require(waiter.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
        waiter.get(); require(entered.load());
        require(mutexes.destroy(other) == 0 && mutexes.available() == 1);
        mutexes.stopDescriptorService();
        require(mutexes.serviceHandle() == 0 && mutexes.available() == 2);
        require(descriptors.lookup(service)); // Original stop bypasses descriptor free.
        const auto again = mutexes.create();
        require(again != 0);
        mutexes.lock(again, 1); mutexes.unlock(again);
    }
    rb::AvsDescriptors small(1);
    rb::AvsMutexes pool(small, 2);
    const auto only = pool.create();
    require(only && pool.available() == 1);
    require(pool.create() == 0 && pool.available() == 1); // Descriptor failure unwinds resource.
    require(pool.destroy(only) == 0 && pool.available() == 2);
    // Real original array descriptor and resource accounting, with native
    // mutex records outside the original 16-byte resource payload.
    {
        rb::AvsDescriptors shared(8);
        rb::AvsResourcePool storage(shared,16,3);
        const auto arrayToken=storage.descriptor();
        require((arrayToken&0x7e0000)==0x20000 && shared.lookup(arrayToken));
        void *borrowed=storage.allocate();
        uint32_t retired;
        {
            rb::AvsMutexes mutexes(shared,storage);
            require(mutexes.available()==2);
            mutexes.registerCurrentThread(1);
            auto first=mutexes.create();retired=mutexes.create();
            require(first && retired && !mutexes.create());
            require(storage.statistics().active==3 && storage.statistics().peakActive==3);
            require(storage.activeIndices()==std::vector<uint32_t>({0,1,2}));
            mutexes.lock(first,1);mutexes.unlock(first);
            mutexes.destroy(first);
            require(storage.freeCount()==1 && !storage.at(1,true));
            first=mutexes.create();require(first && storage.at(1,true));
            require(storage.activeIndices()==std::vector<uint32_t>({0,2,1}));
            mutexes.unregisterCurrentThread();
        }
        require(shared.lookup(arrayToken) && !shared.lookup(retired));
        require(storage.freeCount()==2 && storage.activeIndices()==std::vector<uint32_t>({0}));
        {
            rb::AvsMutexes reused(shared,storage);
            require(reused.create() && reused.available()==1);
        }
        require(storage.freeCount()==2);
        storage.release(borrowed);require(storage.freeCount()==3);
        rb::AvsResourcePool undersized(shared,15,1);
        bool bad=false;try{rb::AvsMutexes invalid(shared,undersized);}catch(const std::invalid_argument&){bad=true;}
        require(bad && undersized.freeCount()==1);
        rb::AvsResourcePool oversized(shared,16,65536);
        bad=false;try{rb::AvsMutexes invalid(shared,oversized);}catch(const std::invalid_argument&){bad=true;}
        require(bad && oversized.freeCount()==65536 && oversized.statistics().active==0);
        rb::AvsDescriptors unrelated(8);
        bad=false;try{rb::AvsMutexes invalid(unrelated,storage);}catch(const std::invalid_argument&){bad=true;}
        require(bad && storage.freeCount()==3);
    }
    {
        rb::AvsDescriptors limited(2);
        rb::AvsResourcePool storage(limited,16,2);
        rb::AvsMutexes mutexes(limited,storage);
        auto token=mutexes.create();require(token && storage.freeCount()==1);
        require(!mutexes.create() && storage.freeCount()==1 && storage.statistics().active==1);
        // Failed descriptor creation still passes through original allocation,
        // so peak usage observes the temporary second resource.
        require(storage.statistics().peakActive==2);
        mutexes.destroy(token);require(storage.freeCount()==2);
    }
    // Race destruction against a newly arriving lock caller. Exactly one
    // operation may win; a rejected destroy must leave the token usable.
    {
        rb::AvsDescriptors raceDescriptors(8);
        rb::AvsMutexes racePool(raceDescriptors, 1);
        for (unsigned iteration = 0; iteration < 512; ++iteration) {
            const auto token = racePool.create(); require(token != 0);
            std::atomic<bool> ready{false}, go{false}, finish{false};
            auto arrival = std::async(std::launch::async, [&] {
                racePool.registerCurrentThread(2);
                ready = true;
                while (!go.load()) std::this_thread::yield();
                bool acquired = false;
                try { racePool.lock(token, 2); acquired = true; }
                catch (const std::invalid_argument &) {} // Destroy won.
                if (acquired) {
                    while (!finish.load()) std::this_thread::yield();
                    racePool.unlock(token);
                }
                racePool.unregisterCurrentThread();
                return acquired;
            });
            while (!ready.load()) std::this_thread::yield();
            go = true;
            if (iteration & 1) std::this_thread::yield();
            bool destroyed = false;
            try { racePool.destroy(token); destroyed = true; }
            catch (const std::logic_error &) {} // Holder/waiter won.
            const bool tokenRetained = raceDescriptors.lookup(token);
            finish = true;
            const bool acquired = arrival.get();
            require(destroyed != acquired);
            require(tokenRetained == !destroyed);
            if (!destroyed) require(racePool.destroy(token) == 0);
            require(racePool.available() == 1);
        }
    }
    bool rejected = false;
    try { rb::AvsMutexes invalid(small, 0); } catch (const std::invalid_argument &) { rejected = true; }
    require(rejected);
    std::cout << "PASS AVS mutex ownership, exhaustion, recursive contention and teardown\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }}
