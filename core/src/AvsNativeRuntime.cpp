#include "rb/AvsNativeRuntime.hpp"
#include "rb/AvsReservedAlarmTask.hpp"
#include "rb/AvsSynchronizationPools.hpp"
#include "rb/AvsTasks.hpp"
#include "rb/AvsThreadContext.hpp"
#include "rb/AvsThreadLocal.hpp"
#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace rb {
namespace {
thread_local const void *activeRuntimeOwner = nullptr;
uint32_t read(const AvsBootConfigImage &image, size_t offset, size_t bytes = 2) {
    uint32_t value = 0;
    for (size_t i = 0; i < bytes; ++i) value |= uint32_t(image[offset+i]) << (8*i);
    return value;
}
}
struct AvsNativeRuntime::Impl {
    std::mutex lifecycle, mutex;
    std::condition_variable wake;
    std::thread owner;
    bool stopping = false;
    Report state;

    void execute(AvsBootConfigImage image, uint32_t mask, uint64_t heapBytes,
                 BeforeStage before, std::promise<Report> ready) {
        Report report;
        bool published = false;
        struct OwnerBinding {
            explicit OwnerBinding(const void *value) { activeRuntimeOwner = value; }
            ~OwnerBinding() { activeRuntimeOwner = nullptr; }
        } ownerBinding(this);
        const auto checkCancellation = [&] {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping) throw std::runtime_error("Native AVS startup cancelled");
        };
        const auto enter = [&](Stage stage) {
            report.stage = stage;
            checkCancellation();
            if (before) before(stage);
            checkCancellation();
        };
        const auto consume = [&](uint64_t amount) {
            if (report.originalArenaBytes > heapBytes || amount > heapBytes - report.originalArenaBytes)
                throw std::runtime_error("AVS original arena exceeds supplied heap");
            report.originalArenaBytes += amount;
        };
        try {
            enter(Stage::descriptors);
            const auto count = read(image,68);
            consume((24ull * (count + 4) + 15) & ~15ull);
            AvsDescriptors descriptors(count);
            report.trace.push_back("Descriptor pool allocated: " + std::to_string(count));

            enter(Stage::platform);
            // Native TLS is a statically allocated C++ TLS slot. The monotonic
            // clock and native recursive synchronization replace Windows TLS,
            // multimedia tick resolution and critical-section spin primitives.
            // No Windows priority-boost or timer-resolution result is invented.
            if (AvsThreadLocal::currentIdentity()) throw std::logic_error("Runtime thread already has AVS TLS");
            const auto plan = planAvsThreadBootstrap(image,mask);
            report.processors = plan.processorCount;
            report.spinCount = plan.spinCount;
            report.trace.push_back("Native thread policy and TLS prepared");

            // Declaration order keeps borrowed arenas alive beyond their owners,
            // even when initialization fails halfway through the original chain.
            std::unique_ptr<AvsResourcePool> taskPool, mutexPool;
            AvsMutexes mutexes(descriptors);
            AvsTaskCompletionRegistry completions;
            enter(Stage::tasks);
            consume(plan.taskArenaBytes);
            taskPool = std::make_unique<AvsResourcePool>(descriptors,AvsTasks::recordSize(),
                plan.threadCount,AvsTasks::recordAlignment());
            report.trace.push_back("Task resource pool and completion registry initialized");

            enter(Stage::workers);
            consume(plan.workerArenaConsumedBytes);
            AvsNativeWorkerPool workers(plan,mutexes);
            AvsTasks tasks(descriptors,workers,plan,*taskPool,&completions);
            report.initialWorkers = uint32_t(workers.initialStartOrder().size());
            report.trace.push_back("Native worker pool started: " + std::to_string(report.initialWorkers));

            enter(Stage::mutexes);
            consume(AvsResourcePool::wrapperArenaBytes(16,read(image,106)));
            mutexPool = std::make_unique<AvsResourcePool>(descriptors,16,read(image,106));
            mutexes.bindResourcePool(*mutexPool,uint16_t(read(image,118)));
            report.trace.push_back("Mutex pool bound with original delay setting");

            enter(Stage::synchronization);
            const uint32_t sizes[] = {20,24,4,20};
            for (size_t i = 0; i < 4; ++i)
                consume(AvsResourcePool::wrapperArenaBytes(sizes[i],read(image,108+2*i)));
            AvsSynchronizationPools synchronization(descriptors,image);
            report.trace.push_back("Condition, event, semaphore and condition-state arenas allocated");

            enter(Stage::alarms);
            consume(40ull * read(image,116)); // 10040BC0 returns start + count * 40.
            AvsAlarms alarms(descriptors,uint16_t(read(image,116)),plan.semaphoreLimit);
            AvsReservedAlarmTask alarmTask(descriptors,mutexes,alarms);
            alarmTask.start();
            const auto alarm = alarmTask.status();
            if (!alarm.taskEntered || alarm.failed) throw std::runtime_error("Reserved alarm worker did not enter its task");
            report.alarmEntered = alarm.taskEntered;
            report.alarmPriorityApplied = alarm.priorityApplied;
            report.trace.push_back("Reserved alarm task 0x40003 entered on its private worker");
            consume((16 - (report.originalArenaBytes & 15)) & 15);

            enter(Stage::descriptorService);
            report.descriptorMutex = mutexes.startDescriptorService();
            report.trace.push_back("Descriptor service mutex started");
            enter(Stage::mainThread);
            AvsNativeThreadPriority priorities;
            AvsThreadContext main(descriptors,mutexes,priorities);
            main.start(read(image,120,4));
            report.mainDescriptor = main.descriptor();
            report.trace.push_back("Original main-thread context bound on the runtime owner");

            enter(Stage::filesystem);
            report.threadPrefixReady = true;
            report.message = "Filesystem bootstrap is not fully recovered. EA3 and game entry were not executed.";
            { std::lock_guard<std::mutex> lock(mutex); state = report; }
            ready.set_value(report); published = true;
            { std::unique_lock<std::mutex> lock(mutex); wake.wait(lock,[&]{return stopping;}); }
            // Native cancellation at an unrecovered boundary. Do not call main
            // complete(0): the original main/game lifecycle has not completed.
            alarmTask.stop();
            workers.stop();
            mutexes.stopDescriptorService();
        } catch (const std::exception &error) {
            report.failed = true; report.threadPrefixReady = false; report.message = error.what();
        } catch (...) {
            report.failed = true; report.threadPrefixReady = false; report.message = "Unknown native startup failure";
        }
        // The trace/arena extent are historical evidence; live readiness and
        // descriptor handles must not survive their native owners.
        report.threadPrefixReady = false;
        report.mainDescriptor = report.descriptorMutex = report.initialWorkers = 0;
        report.alarmEntered = report.alarmPriorityApplied = false;
        if (!report.failed) report.message = "Native AVS runtime stopped at the unrecovered filesystem boundary.";
        { std::lock_guard<std::mutex> lock(mutex); state = report; }
        if (!published) ready.set_value(report);
    }
};
AvsNativeRuntime::AvsNativeRuntime() : impl_(new Impl) {}
AvsNativeRuntime::~AvsNativeRuntime() { stop(); }
AvsNativeRuntime::Report AvsNativeRuntime::start(const AvsBootConfigImage &image,
        uint32_t mask, uint64_t bytes, BeforeStage before) {
    std::promise<Report> ready; auto result = ready.get_future();
    {
        std::lock_guard<std::mutex> lifecycle(impl_->lifecycle);
        if (impl_->owner.joinable()) throw std::logic_error("Stop the existing AVS runtime before starting another");
        { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->stopping = false; impl_->state = {}; }
        impl_->owner = std::thread(&Impl::execute,impl_.get(),image,mask,bytes,std::move(before),std::move(ready));
    }
    return result.get();
}
void AvsNativeRuntime::requestStop() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stopping = true; impl_->wake.notify_all();
}
void AvsNativeRuntime::stop() {
    if (activeRuntimeOwner == impl_.get())
        throw std::logic_error("AVS runtime cannot join its own thread");
    std::lock_guard<std::mutex> lifecycle(impl_->lifecycle);
    if (!impl_->owner.joinable()) return;
    requestStop();
    impl_->owner.join();
}
AvsNativeRuntime::Report AvsNativeRuntime::report() const {
    std::lock_guard<std::mutex> lock(impl_->mutex); return impl_->state;
}
uint32_t AvsNativeRuntime::nativeProcessAffinityMask() {
#if defined(_WIN32)
    DWORD_PTR process = 0, system = 0;
    if (!GetProcessAffinityMask(GetCurrentProcess(),&process,&system))
        throw std::runtime_error("Cannot query process affinity");
    return uint32_t(process);
#else
    const auto count = std::min(32u,std::thread::hardware_concurrency());
    if (!count) throw std::runtime_error("Cannot query native processor count");
    return count == 32 ? UINT32_MAX : (uint32_t(1) << count) - 1;
#endif
}
const char *AvsNativeRuntime::address(Stage stage) noexcept {
    switch(stage) {
    case Stage::descriptors: return "libavs-win32.dll:export103";
    case Stage::platform: return "libavs-win32.dll:0x10039530";
    case Stage::tasks: case Stage::workers: return "libavs-win32.dll:0x1001ABC0";
    case Stage::mutexes: return "libavs-win32.dll:0x10022B80";
    case Stage::synchronization: return "libavs-win32.dll:0x1000EC00-0x1001BF70";
    case Stage::alarms: return "libavs-win32.dll:0x10040700";
    case Stage::descriptorService: return "libavs-win32.dll:export013";
    case Stage::mainThread: return "libavs-win32.dll:0x1001AA40";
    case Stage::filesystem: return "libavs-win32.dll:0x10022CD0";
    }
    return "Unknown AVS stage";
}
} // namespace rb
