#include "rb/AvsNativeWorkerPool.hpp"
#include <condition_variable>
#include <atomic>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <system_error>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <sched.h>
#endif

namespace rb {
namespace { thread_local uintptr_t workerIdentity = 0; }
struct AvsNativeWorkerPool::Impl {
    struct Work {
        Task action;
        Completion completion;
        std::promise<int32_t> result;
    };
    struct Worker {
        Impl *owner = nullptr;
        mutable std::mutex mutex;
        std::condition_variable ready;
        uint16_t state = 0;
        uint32_t remaining = 0;
        int32_t affinity = -1;
        bool affinityApplied = false, started = false, gate = false, entered = false, finished = false;
        std::unique_ptr<Work> work;
        std::exception_ptr failure;
        std::exception_ptr cleanupFailure;
#if defined(_WIN32)
        HANDLE thread = nullptr;
#else
        pthread_t thread{};
#endif
    };
    AvsMutexes &mutexes;
    std::vector<std::unique_ptr<Worker>> workers;
    std::vector<size_t> order;
    std::mutex lifecycleMutex;
    std::atomic<bool> stopping{false};
    explicit Impl(AvsMutexes &value) : mutexes(value) {}
    static void execute(Worker &worker) noexcept {
        bool registered = false;
        try {
            std::unique_lock<std::mutex> lock(worker.mutex);
            worker.ready.wait(lock, [&]{return worker.gate;});
            workerIdentity = reinterpret_cast<uintptr_t>(&worker);
            worker.owner->mutexes.registerCurrentThread(workerIdentity);
            registered = true;
            worker.entered = true;
            worker.ready.notify_all();
            for (;;) {
                worker.ready.wait(lock, [&]{ return worker.state != 1 && worker.state != 2; });
                if (worker.state == 0 || worker.state == 4) break;
                if (worker.state != 3 || !worker.work)
                    throw std::logic_error("AVS worker has an unsupported dispatch state");
                auto work = std::move(worker.work);
                lock.unlock();
                try {
                    const auto result = work->action(); // 10041130 task execution.
                    for (auto &callback : work->completion) if (callback) callback();
                    worker.owner->mutexes.checkCurrentThreadCanUnregister();
                    work->result.set_value(result); // Completion follows callbacks, 10041230.
                } catch (...) {
                    const auto failure = std::current_exception();
                    work->result.set_exception(failure);
                    worker.owner->mutexes.releaseCurrentThreadLocksAfterFailure();
                    lock.lock(); worker.failure = failure; worker.state = 0;
                    worker.ready.notify_all(); break;
                }
                lock.lock();
                if (worker.state == 3) {
                    if (worker.remaining == 0 || --worker.remaining != 0) worker.state = 1;
                    else worker.state = 0;
                }
                worker.ready.notify_all();
            }
            worker.owner->mutexes.unregisterCurrentThread();
            registered = false;
            workerIdentity = 0;
            worker.finished = true;
            worker.ready.notify_all();
        } catch (...) {
            const auto failure = std::current_exception();
            std::exception_ptr cleanupFailure;
            if (registered) {
                try {
                    worker.owner->mutexes.releaseCurrentThreadLocksAfterFailure();
                    worker.owner->mutexes.unregisterCurrentThread(); registered = false;
                } catch (...) { cleanupFailure = std::current_exception(); }
            }
            std::lock_guard<std::mutex> lock(worker.mutex);
            worker.failure = failure; worker.state = 0;
            worker.cleanupFailure = cleanupFailure;
            worker.finished = true;
            if (worker.work) worker.work->result.set_exception(worker.failure);
            worker.work.reset(); worker.ready.notify_all(); workerIdentity = 0;
        }
    }
#if defined(_WIN32)
    static unsigned __stdcall entry(void *argument) { execute(*static_cast<Worker *>(argument)); return 0; }
#else
    static void *entry(void *argument) { execute(*static_cast<Worker *>(argument)); return nullptr; }
#endif
    void start(Worker &worker, const AvsThreadBootstrapPlan &plan, int32_t affinity) {
        worker.remaining = plan.expiryCount; worker.affinity = affinity; worker.state = 1;
        if (plan.workerPriority > 4) throw std::invalid_argument("AVS worker priority must be 0 through 4");
#if defined(_WIN32)
        const auto thread = _beginthreadex(nullptr, plan.workerStack, entry, &worker, 0, nullptr);
        if (!thread) { worker.state = 0; throw std::runtime_error("AVS worker native creation failed"); }
        worker.thread = reinterpret_cast<HANDLE>(thread); worker.started = true;
        SetThreadPriorityBoost(worker.thread, TRUE);
        if (!SetThreadPriority(worker.thread, 2-int(plan.workerPriority))) {
            { std::lock_guard<std::mutex> lock(worker.mutex); worker.state = 0; worker.gate = true; }
            worker.ready.notify_all();
            throw std::runtime_error("AVS worker native priority failed");
        }
        if (affinity >= 0 && uint32_t(affinity) < sizeof(DWORD_PTR)*8)
            worker.affinityApplied = SetThreadAffinityMask(worker.thread, DWORD_PTR(1) << affinity) != 0;
#else
        if (plan.workerPriority != 2)
            throw std::runtime_error("AVS worker relative priority has no exact native mapping");
        pthread_attr_t attributes;
        int error = pthread_attr_init(&attributes);
        if (error) throw std::system_error(error, std::generic_category(), "AVS worker attributes");
        if (plan.workerStack) error = pthread_attr_setstacksize(&attributes, plan.workerStack);
        if (!error) error = pthread_create(&worker.thread, &attributes, entry, &worker);
        pthread_attr_destroy(&attributes);
        if (error) { worker.state = 0; throw std::system_error(error, std::generic_category(), "AVS worker native creation"); }
        worker.started = true;
        sched_param priority{};
        error = pthread_setschedparam(worker.thread, SCHED_OTHER, &priority);
        if (error) {
            { std::lock_guard<std::mutex> lock(worker.mutex); worker.state = 0; worker.gate = true; }
            worker.ready.notify_all();
            throw std::system_error(error, std::generic_category(), "AVS worker native priority");
        }
#if defined(__linux__)
        if (affinity >= 0 && affinity < CPU_SETSIZE) {
            cpu_set_t set; CPU_ZERO(&set); CPU_SET(affinity, &set);
            worker.affinityApplied = pthread_setaffinity_np(worker.thread, sizeof(set), &set) == 0;
        }
#endif
        // Darwin has no equivalent hard CPU pinning API. The original native
        // create wrapper ignores affinity-setting failure; expose that status.
#endif
        { std::lock_guard<std::mutex> lock(worker.mutex); worker.gate = true; }
        worker.ready.notify_one();
        { std::unique_lock<std::mutex> lock(worker.mutex);
          worker.ready.wait(lock,[&]{return worker.entered || worker.failure;});
          if (worker.failure) std::rethrow_exception(worker.failure); }
    }
    void stop() {
        for (const auto &worker : workers)
            if (workerIdentity == reinterpret_cast<uintptr_t>(worker.get()))
                throw std::logic_error("AVS worker cannot join its own pool");
        std::lock_guard<std::mutex> lifecycle(lifecycleMutex);
        stopping = true;
        for (auto &worker : workers) {
            std::lock_guard<std::mutex> lock(worker->mutex);
            worker->state = 0; worker->gate = true;
            if (worker->work) {
                worker->work->result.set_exception(std::make_exception_ptr(std::runtime_error("AVS worker stopped before dispatch")));
                worker->work.reset();
            }
            worker->ready.notify_all();
        }
        // 41E40 unwind destroys initialized records from highest index down.
        for (auto it = workers.rbegin(); it != workers.rend(); ++it) {
            auto &worker = **it;
            if (!worker.started) continue;
#if defined(_WIN32)
            if (WaitForSingleObject(worker.thread, INFINITE) != WAIT_OBJECT_0)
                throw std::system_error(int(GetLastError()), std::system_category(), "AVS worker join failed");
            if (!CloseHandle(worker.thread))
                throw std::system_error(int(GetLastError()), std::system_category(), "AVS worker handle close failed");
            worker.thread = nullptr;
#else
            const int error = pthread_join(worker.thread, nullptr);
            if (error) throw std::system_error(error, std::generic_category(), "AVS worker join failed");
#endif
            { std::lock_guard<std::mutex> lock(worker.mutex); worker.started = false; }
        }
        for (const auto &worker : workers) {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (worker->cleanupFailure) std::rethrow_exception(worker->cleanupFailure);
        }
    }
};
AvsNativeWorkerPool::AvsNativeWorkerPool(const AvsThreadBootstrapPlan &plan, AvsMutexes &mutexes)
    : impl_(std::make_unique<Impl>(mutexes)) {
    if (!plan.threadCount) throw std::invalid_argument("AVS worker capacity must be nonzero");
    try {
        for (uint32_t i=0;i<plan.threadCount;++i) {
            auto worker=std::make_unique<Impl::Worker>();worker->owner=impl_.get();impl_->workers.push_back(std::move(worker));
        }
        AvsThreadAffinity affinity(plan.affinityMask);
        for (uint32_t i=0;i<plan.initialCount && i<plan.threadCount;++i) {
            const auto index=plan.threadCount-1-i;
            impl_->start(*impl_->workers[index],plan,affinity.next());impl_->order.push_back(index);
        }
    } catch (...) { impl_->stop(); throw; }
}
AvsNativeWorkerPool::~AvsNativeWorkerPool() { impl_->stop(); }
std::future<int32_t> AvsNativeWorkerPool::assign(size_t slot, Task action, Completion callbacks) {
    if (!action) throw std::invalid_argument("AVS worker action is missing");
    auto &worker=*impl_->workers.at(slot);
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (impl_->stopping || !worker.started || worker.state!=1 || worker.work) throw std::logic_error("AVS worker slot is not idle");
    worker.work=std::make_unique<Impl::Work>();worker.work->action=std::move(action);worker.work->completion=std::move(callbacks);
    auto result=worker.work->result.get_future();worker.state=3;worker.ready.notify_one();return result;
}
void AvsNativeWorkerPool::waitIdle(size_t slot) {
    auto &worker=*impl_->workers.at(slot);
    if (workerIdentity == reinterpret_cast<uintptr_t>(&worker))
        throw std::logic_error("AVS worker cannot wait for its own task to finish");
    std::unique_lock<std::mutex> lock(worker.mutex);
    worker.ready.wait(lock,[&]{return worker.state==1||((worker.state==0||worker.state==4)&&(worker.finished||!worker.started));});
    if (worker.failure) std::rethrow_exception(worker.failure);
}
AvsNativeWorkerPool::WorkerStatus AvsNativeWorkerPool::status(size_t slot) const {
    auto &worker=*impl_->workers.at(slot);std::lock_guard<std::mutex> lock(worker.mutex);
    return {worker.state,worker.remaining,worker.affinity,worker.affinityApplied,worker.started,bool(worker.failure)};
}
const std::vector<size_t> &AvsNativeWorkerPool::initialStartOrder() const noexcept { return impl_->order; }
void AvsNativeWorkerPool::stop() { impl_->stop(); }
uintptr_t AvsNativeWorkerPool::currentWorkerIdentity() noexcept { return workerIdentity; }
} // namespace rb
