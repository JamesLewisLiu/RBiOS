#include "rb/AvsThreadContext.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("AVS thread context regression"); }
struct Priority : rb::AvsThreadPriorityBackend {
    int32_t status = 0; std::vector<int32_t> calls;
    int32_t setCurrentPriority(int32_t value) override { calls.push_back(value); return status; }
};
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--map") {
        for (uint32_t p = 0; p < 5; ++p) std::cout << rb::AvsThreadContext::mapPriority(p) << '\n';
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--start") {
        for (uint32_t p = 0; p < 5; ++p) {
            rb::AvsDescriptors descriptors(8); rb::AvsMutexes mutexes(descriptors, 4); Priority backend;
            rb::AvsThreadContext context(descriptors, mutexes, backend); context.start(p);
            std::cout << context.descriptor() << ' ' << context.priority() << ' ' << backend.calls.size()
                      << ' ' << (backend.calls.empty() ? 0 : backend.calls[0]) << ' '
                      << (rb::AvsThreadContext::currentIdentity() != 0) << '\n';
            context.complete(0); context.stop();
        } return 0;
    }
    require(rb::AvsThreadContext::currentIdentity() == 0);
    rb::AvsDescriptors descriptors(8); rb::AvsMutexes mutexes(descriptors, 4);
    for (uint32_t priority = 0; priority < 5; ++priority) {
        Priority backend;
        rb::AvsThreadContext context(descriptors, mutexes, backend);
        context.start(priority);
        require(context.started() && context.descriptor() && context.priority() == priority);
        const auto identity = rb::AvsThreadContext::currentIdentity(); require(identity != 0);
        auto isolated = std::async(std::launch::async, [] { return rb::AvsThreadContext::currentIdentity(); });
        require(isolated.get() == 0);
        require(backend.calls == (priority ? std::vector<int32_t>{2-int32_t(priority)} : std::vector<int32_t>{}));
        const auto mutex = mutexes.create();
        mutexes.lock(mutex, identity); mutexes.unlock(mutex); mutexes.destroy(mutex);
        bool rejected = false;
        try { context.stop(); } catch (const std::logic_error &) { rejected = true; }
        require(rejected && context.started());
        context.complete(123); context.stop();
        require(!context.started() && rb::AvsThreadContext::currentIdentity() == 0);
    }
    {
        Priority failure; failure.status = -1;
        rb::AvsThreadContext context(descriptors, mutexes, failure);
        bool rejected = false;
        try { context.start(2); } catch (const std::runtime_error &) { rejected = true; }
        require(rejected && !context.started() && rb::AvsThreadContext::currentIdentity() != 0);
    }
    require(rb::AvsThreadContext::currentIdentity() == 0);
    {
        Priority backend;
        rb::AvsThreadContext context(descriptors, mutexes, backend);
        context.start(2);
        const auto identity = rb::AvsThreadContext::currentIdentity();
        const auto token = context.descriptor();
        const auto ownedMutex = mutexes.create();
        mutexes.lock(ownedMutex, identity);
        context.complete(0);
        bool rejected = false;
        try { context.stop(); } catch (const std::logic_error &) { rejected = true; }
        require(rejected && context.started() && descriptors.lookup(token));
        require(rb::AvsThreadContext::currentIdentity() == identity);
        mutexes.unlock(ownedMutex); mutexes.destroy(ownedMutex);
        auto wrongThread = std::async(std::launch::async, [&] {
            try { context.stop(); } catch (const std::logic_error &) { return true; }
            return false;
        });
        require(wrongThread.get());
        require(context.started() && descriptors.lookup(token));
        require(rb::AvsThreadContext::currentIdentity() == identity);
        context.stop();
        require(!context.started() && !descriptors.lookup(token));
    }
    {
        rb::AvsNativeThreadPriority native;
        rb::AvsThreadContext context(descriptors, mutexes, native);
        context.start(2); context.complete(0); context.stop();
    }
    {
        rb::AvsDescriptors small(1); rb::AvsMutexes resource(small, 1);
        small.allocate(7, nullptr, 0); Priority backend;
        rb::AvsThreadContext context(small, resource, backend);
        bool rejected = false;
        try { context.start(2); } catch (const std::runtime_error &) { rejected = true; }
        require(rejected && backend.calls.empty() && !context.started());
    }
    require(rb::AvsThreadContext::currentIdentity() == 0);
    std::cout << "PASS AVS current-thread TLS, descriptor ownership, priority and explicit completion\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }}
