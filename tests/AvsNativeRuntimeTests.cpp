#include "rb/AvsNativeRuntime.hpp"
#include "rb/AvsConfigImport.hpp"
#include "rb/AvsThreadLocal.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using Runtime = rb::AvsNativeRuntime;
using Stage = Runtime::Stage;
void require(bool ok) { if (!ok) throw std::runtime_error("AVS runtime integration regression"); }
void put(rb::AvsBootConfigImage &image, size_t offset, uint32_t value, size_t bytes = 2) {
    for (size_t i = 0; i < bytes; ++i) image[offset+i] = uint8_t(value >> (8*i));
}
auto config() {
    auto image = rb::importAvsBootConfig();
    put(image,104,4); put(image,88,2);
    put(image,72,0,4); put(image,76,0,4);
    return image;
}
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--boot-batch") {
        uint32_t counts[8];
        while (std::cin >> counts[0] >> counts[1] >> counts[2] >> counts[3]
               >> counts[4] >> counts[5] >> counts[6] >> counts[7]) {
            auto image = config();
            put(image,68,counts[0]); put(image,88,0);
            for (size_t i = 1; i < 8; ++i) put(image,102+2*i,counts[i]);
            Runtime runtime; const auto result = runtime.start(image,1,0x1000000);
            if (result.failed) std::cout << "error " << result.message << '\n';
            else std::cout << result.originalArenaBytes << ' ' << result.descriptorMutex << ' '
                           << result.mainDescriptor << '\n';
            runtime.stop();
        }
        return 0;
    }
    Runtime runtime;
    std::vector<Stage> order;
    std::thread::id owner;
    const auto caller = std::this_thread::get_id();
    auto state = runtime.start(config(),0xff,0x1000000,[&](Stage stage) {
        if (owner == std::thread::id{}) owner = std::this_thread::get_id();
        require(owner == std::this_thread::get_id() && owner != caller);
        order.push_back(stage);
        if (stage == Stage::filesystem) require(rb::AvsThreadLocal::currentDescriptor() != 0);
    });
    require(state.threadPrefixReady && !state.failed && state.stage == Stage::filesystem);
    require(state.alarmEntered && state.mainDescriptor && state.descriptorMutex);
    require(state.initialWorkers == 2 && state.processors == 8 && state.spinCount == 4000);
    require(state.originalArenaBytes < 0x1000000 && !(state.originalArenaBytes & 15));
    require(!rb::AvsThreadLocal::currentIdentity());
    require(order == std::vector<Stage>({Stage::descriptors,Stage::platform,Stage::tasks,
        Stage::workers,Stage::mutexes,Stage::synchronization,Stage::alarms,
        Stage::descriptorService,Stage::mainThread,Stage::filesystem}));
    // Tokens expose actual source allocation order: six resource pools,
    // descriptor-service mutex, then the main task; alarm uses reserved slot 3.
    require((state.descriptorMutex & 0x1ffff) == 10 && (state.mainDescriptor & 0x1ffff) == 11);
    bool rejected = false;
    try { runtime.start(config(),1,0x1000000); } catch (const std::logic_error &) { rejected = true; }
    require(rejected);
    std::async(std::launch::async,[&] { runtime.stop(); }).get();
    require(!runtime.report().failed);
    require(!runtime.report().threadPrefixReady && !runtime.report().mainDescriptor &&
        !runtime.report().descriptorMutex && !runtime.report().alarmEntered);
    runtime.stop();
    state = runtime.start(config(),1,0x1000000,[&](Stage stage) {
        if (stage == Stage::workers) runtime.stop();
    });
    require(state.failed && state.message == "AVS runtime cannot join its own thread");
    runtime.stop();
    std::promise<void> reached, proceed;
    auto proceedFuture = proceed.get_future();
    auto pending = std::async(std::launch::async,[&] {
        return runtime.start(config(),1,0x1000000,[&](Stage stage) {
            if (stage == Stage::descriptorService) { reached.set_value(); proceedFuture.wait(); }
        });
    });
    reached.get_future().get(); runtime.requestStop(); proceed.set_value();
    state = pending.get(); runtime.stop();
    require(state.failed && !state.threadPrefixReady && state.stage == Stage::descriptorService);
    require(state.message == "Native AVS startup cancelled" && !state.mainDescriptor && !state.alarmEntered);
    // Fail at every actual boundary, including after a private alarm worker has
    // started and after main TLS binds. Every attempt must join and be retryable.
    for (const auto stage : order) {
        state = runtime.start(config(),3,0x1000000,[=](Stage current) {
            if (current == stage) throw std::runtime_error("Injected stage failure");
        });
        require(state.failed && !state.threadPrefixReady && state.stage == stage);
        require(state.message == "Injected stage failure");
        runtime.stop();
        require(!rb::AvsThreadLocal::currentIdentity());
    }
    // Real resource/descriptor/configuration exhaustion, not success callbacks.
    for (size_t offset : {68u,104u,106u,108u,110u,112u,114u,116u}) {
        auto image = config(); put(image,offset,0);
        state = runtime.start(image,1,0x1000000);
        require(state.failed && !state.threadPrefixReady); runtime.stop();
    }
    auto image = config(); put(image,68,6); // Resource pools exhaust all descriptors.
    state = runtime.start(image,1,0x1000000);
    require(state.failed && state.stage == Stage::descriptorService); runtime.stop();
    put(image,68,7);
    state = runtime.start(image,1,0x1000000);
    require(state.failed && state.stage == Stage::mainThread); runtime.stop();
    image = config(); put(image,120,5,4);
    state = runtime.start(image,1,0x1000000);
    require(state.failed && state.stage == Stage::mainThread); runtime.stop();
    state = runtime.start(config(),1,32);
    require(state.failed && state.stage == Stage::descriptors); runtime.stop();
    for (int i = 0; i < 8; ++i) {
        state = runtime.start(config(),1,0x1000000);
        require(state.threadPrefixReady && !state.failed); runtime.stop();
        require(!runtime.report().failed);
    }
    require(Runtime::nativeProcessAffinityMask() != 0);
    std::cout << "PASS original thread boot order, dedicated owner, every stage unwind and repeated startup\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; } }
