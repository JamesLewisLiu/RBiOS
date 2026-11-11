#include "rb/AvsNativeWorkerPool.hpp"
#include <condition_variable>
#include <atomic>
#include <algorithm>
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
#include <unistd.h>
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
        uint32_t stackSize = 0, priority = 2;
        bool priorityApplied = false;
        size_t index = 0;
        enum class Place { free, idle, checkedOut, running, retired, lost } place = Place::free;
        int32_t affinity = -1;
        bool affinityApplied = false, started = false, gate = false, entered = false, finished = false, idlePublished = false;
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
    std::mutex shutdownMutex;
    std::atomic<bool> stopping{false};
    std::recursive_mutex poolMutex;
    std::vector<size_t> freeSlots, idleSlots, retiredSlots;
    AvsThreadBootstrapPlan defaults;
    AvsThreadAffinity affinity;
    explicit Impl(AvsMutexes &value, const AvsThreadBootstrapPlan &plan) : mutexes(value), defaults(plan), affinity(plan.affinityMask) {}
    void returnedIdle(Worker &worker) {
        std::lock_guard<std::recursive_mutex> pool(poolMutex);
        std::lock_guard<std::mutex> lock(worker.mutex);
        if (worker.state == 1 && !stopping && worker.place == Worker::Place::running) {
            worker.place = Worker::Place::idle; idleSlots.push_back(worker.index);
            worker.idlePublished = true;
        }
    }
    void retired(Worker &worker) {
        std::lock_guard<std::recursive_mutex> pool(poolMutex);
        if (worker.place != Worker::Place::retired) {
            worker.place = Worker::Place::retired; retiredSlots.push_back(worker.index);
            affinity.release(worker.affinity);
        }
    }
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
                const bool idle = worker.state == 1;
                lock.unlock(); if (idle) worker.owner->returnedIdle(worker); lock.lock();
                worker.ready.notify_all();
            }
            const bool retire = worker.state == 0;
            lock.unlock(); if (retire) worker.owner->retired(worker); lock.lock();
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
        std::unique_lock<std::mutex> lock(worker.mutex);
        worker.remaining = plan.expiryCount; worker.affinity = affinity; worker.state = 1;
        worker.stackSize = plan.workerStack; worker.priority = plan.workerPriority;
        worker.gate = false; worker.entered = false; worker.finished = false;
        worker.failure = {}; worker.cleanupFailure = {}; worker.affinityApplied = false;
        worker.idlePublished = false;
        if (plan.workerPriority > 4) throw std::invalid_argument("AVS worker priority must be 0 through 4");
#if defined(_WIN32)
        const auto thread = _beginthreadex(nullptr, plan.workerStack, entry, &worker, 0, nullptr);
        if (!thread) { worker.state = 0; throw std::runtime_error("AVS worker native creation failed"); }
        worker.thread = reinterpret_cast<HANDLE>(thread); worker.started = true;
        SetThreadPriorityBoost(worker.thread, TRUE);
        worker.priorityApplied = SetThreadPriority(worker.thread, 2-int(plan.workerPriority)) != 0;
        if (affinity >= 0 && uint32_t(affinity) < sizeof(DWORD_PTR)*8)
            worker.affinityApplied = SetThreadAffinityMask(worker.thread, DWORD_PTR(1) << affinity) != 0;
#else
        pthread_attr_t attributes;
        int error = pthread_attr_init(&attributes);
        if (error) throw std::system_error(error, std::generic_category(), "AVS worker attributes");
        if (plan.workerStack) {
            const long pageQuery = sysconf(_SC_PAGESIZE);
            const size_t pageSize = pageQuery > 0 ? size_t(pageQuery) : size_t(4096);
            const size_t requested = std::max<size_t>(plan.workerStack, PTHREAD_STACK_MIN);
            const size_t allocated = (requested + pageSize - 1) / pageSize * pageSize;
            error = pthread_attr_setstacksize(&attributes, allocated);
        }
        if (!error) error = pthread_create(&worker.thread, &attributes, entry, &worker);
        pthread_attr_destroy(&attributes);
        if (error) { worker.state = 0; throw std::system_error(error, std::generic_category(), "AVS worker native creation"); }
        worker.started = true;
        sched_param priority{};
        worker.priorityApplied = plan.workerPriority == 2 && pthread_setschedparam(worker.thread, SCHED_OTHER, &priority) == 0;
#if defined(__linux__)
        if (affinity >= 0 && affinity < CPU_SETSIZE) {
            cpu_set_t set; CPU_ZERO(&set); CPU_SET(affinity, &set);
            worker.affinityApplied = pthread_setaffinity_np(worker.thread, sizeof(set), &set) == 0;
        }
#endif
        // Darwin has no equivalent hard CPU pinning API. The original native
        // create wrapper ignores affinity-setting failure; expose that status.
#endif
        worker.gate = true;
        worker.ready.notify_one();
        worker.ready.wait(lock,[&]{return worker.entered || worker.failure;});
        if (worker.failure) std::rethrow_exception(worker.failure);
    }
    void join(Worker &worker) {
        if (!worker.started) return;
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
        std::lock_guard<std::mutex> lock(worker.mutex);worker.started=false;
    }
    void changePriority(Worker &worker, uint32_t priority) {
        std::lock_guard<std::mutex> lock(worker.mutex);
        if (worker.priority == priority) return;
        worker.priority = priority;
#if defined(_WIN32)
        worker.priorityApplied = SetThreadPriority(worker.thread,2-int(priority)) != 0;
#else
        sched_param value{};
        worker.priorityApplied = priority==2 && pthread_setschedparam(worker.thread,SCHED_OTHER,&value)==0;
#endif
    }
    void stop() {
        for (const auto &worker : workers)
            if (workerIdentity == reinterpret_cast<uintptr_t>(worker.get()))
                throw std::logic_error("AVS worker cannot join its own pool");
        std::lock_guard<std::mutex> shutdown(shutdownMutex);
        {
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
        }
        // Running tasks may call checkout while draining. They must acquire
        // lifecycleMutex and observe stopping, not block behind their own join.
        // 41E40 unwind destroys initialized records from highest index down.
        for (auto it = workers.rbegin(); it != workers.rend(); ++it) {
            auto &worker = **it;
            join(worker);
        }
        for (const auto &worker : workers) {
            std::lock_guard<std::mutex> lock(worker->mutex);
            if (worker->cleanupFailure) std::rethrow_exception(worker->cleanupFailure);
        }
    }
};
AvsNativeWorkerPool::AvsNativeWorkerPool(const AvsThreadBootstrapPlan &plan, AvsMutexes &mutexes)
    : impl_(std::make_unique<Impl>(mutexes,plan)) {
    if (!plan.threadCount) throw std::invalid_argument("AVS worker capacity must be nonzero");
    try {
        for (uint32_t i=0;i<plan.threadCount;++i) {
            auto worker=std::make_unique<Impl::Worker>();worker->owner=impl_.get();worker->index=i;
            impl_->workers.push_back(std::move(worker));impl_->freeSlots.push_back(i);
        }
        for (uint32_t i=0;i<plan.initialCount && i<plan.threadCount;++i) {
            const auto index=impl_->freeSlots.back();impl_->freeSlots.pop_back();
            auto &worker=*impl_->workers[index];worker.place=Impl::Worker::Place::checkedOut;
            impl_->start(worker,plan,impl_->affinity.next());impl_->order.push_back(index);
            {std::lock_guard<std::mutex> lock(worker.mutex);worker.place=Impl::Worker::Place::idle;worker.idlePublished=true;}
            impl_->idleSlots.push_back(index);
        }
    } catch (...) { impl_->stop(); throw; }
}
AvsNativeWorkerPool::~AvsNativeWorkerPool() { impl_->stop(); }
std::optional<size_t> AvsNativeWorkerPool::checkout(uint32_t stackSize, uint32_t priority, uint32_t expiry) {
    if (priority>4) throw std::invalid_argument("AVS checkout priority must be 0 through 4");
    std::lock_guard<std::mutex> lifecycle(impl_->lifecycleMutex);
    if (impl_->stopping) return std::nullopt;
    const auto defaultStack=impl_->defaults.workerStack;
    if (!stackSize) {
        stackSize=defaultStack;priority=impl_->defaults.workerPriority;
        expiry=defaultStack?impl_->defaults.expiryCount:1;
    }
    std::unique_lock<std::recursive_mutex> pool(impl_->poolMutex);
    if (stackSize<=defaultStack && !impl_->idleSlots.empty()) {
        const auto slot=impl_->idleSlots.back();impl_->idleSlots.pop_back();
        auto &worker=*impl_->workers[slot];
        {
            std::lock_guard<std::mutex> lock(worker.mutex);
            if(worker.state!=1) {
                // Original 41D80 loops on the same non-idle record indefinitely.
                // Report that invalid native state instead of hanging the host.
                worker.place=Impl::Worker::Place::lost;
                throw std::logic_error("AVS idle list contains a non-idle worker");
            }
            worker.state=2;worker.idlePublished=false;worker.place=Impl::Worker::Place::checkedOut;
            worker.ready.notify_one();
        }
        pool.unlock();impl_->changePriority(worker,priority);return slot;
    }
    while(!impl_->retiredSlots.empty()) {
        const auto slot=impl_->retiredSlots.back();impl_->retiredSlots.pop_back();
        auto &worker=*impl_->workers[slot];impl_->join(worker);
        if(worker.cleanupFailure)std::rethrow_exception(worker.cleanupFailure);
        worker.place=Impl::Worker::Place::free;impl_->freeSlots.push_back(slot);
    }
    size_t slot;
    bool restart=false;
    if(!impl_->freeSlots.empty()) {
        slot=impl_->freeSlots.back();impl_->freeSlots.pop_back();
        if(stackSize>defaultStack)expiry=1;
    } else if(stackSize>defaultStack&&!impl_->idleSlots.empty()) {
        slot=impl_->idleSlots.back();impl_->idleSlots.pop_back();
        impl_->affinity.release(impl_->workers[slot]->affinity);restart=true;expiry=1;
    } else return std::nullopt;
    auto &worker=*impl_->workers[slot];worker.place=Impl::Worker::Place::checkedOut;
    const auto affinity=impl_->affinity.next();pool.unlock();
    try {
        if(restart) {
            {std::lock_guard<std::mutex> lock(worker.mutex);worker.state=4;worker.idlePublished=false;worker.ready.notify_one();}
            impl_->join(worker);
        }
        auto plan=impl_->defaults;plan.workerStack=stackSize;plan.workerPriority=priority;plan.expiryCount=expiry;
        impl_->start(worker,plan,affinity);return slot;
    } catch (...) {
        const auto failure=std::current_exception();
        if(worker.started)impl_->join(worker);
        {std::lock_guard<std::recursive_mutex> list(impl_->poolMutex);worker.place=Impl::Worker::Place::lost;}
        {std::lock_guard<std::mutex> lock(worker.mutex);worker.failure=failure;worker.state=0;worker.finished=true;}
        // Original checkout does not return the removed record or decrement
        // affinity after native creation fails. Preserve those side effects.
        return std::nullopt;
    }
}
std::future<int32_t> AvsNativeWorkerPool::assign(size_t slot, Task action, Completion callbacks) {
    if (!action) throw std::invalid_argument("AVS worker action is missing");
    auto work=std::make_unique<Impl::Work>();work->action=std::move(action);work->completion=std::move(callbacks);
    auto result=work->result.get_future();
    auto &worker=*impl_->workers.at(slot);
    std::lock_guard<std::recursive_mutex> pool(impl_->poolMutex);
    std::lock_guard<std::mutex> lock(worker.mutex);
    if (impl_->stopping || !worker.started || (worker.state!=1&&worker.state!=2) || worker.work ||
        (worker.place!=Impl::Worker::Place::idle&&worker.place!=Impl::Worker::Place::checkedOut))
        throw std::logic_error("AVS worker slot is not idle or checked out");
    if(worker.place==Impl::Worker::Place::idle) {
        auto found=std::find(impl_->idleSlots.begin(),impl_->idleSlots.end(),slot);
        if(found==impl_->idleSlots.end())throw std::logic_error("AVS idle list does not own worker");
        impl_->idleSlots.erase(found);
    }
    worker.work=std::move(work);worker.state=3;worker.idlePublished=false;
    worker.place=Impl::Worker::Place::running;worker.ready.notify_one();return result;
}
void AvsNativeWorkerPool::waitIdle(size_t slot) {
    auto &worker=*impl_->workers.at(slot);
    if (workerIdentity == reinterpret_cast<uintptr_t>(&worker))
        throw std::logic_error("AVS worker cannot wait for its own task to finish");
    std::unique_lock<std::mutex> lock(worker.mutex);
    worker.ready.wait(lock,[&]{return (worker.state==1&&worker.idlePublished)||((worker.state==0||worker.state==4)&&(worker.finished||!worker.started));});
    if (worker.failure) std::rethrow_exception(worker.failure);
}
AvsNativeWorkerPool::WorkerStatus AvsNativeWorkerPool::status(size_t slot) const {
    auto &worker=*impl_->workers.at(slot);std::lock_guard<std::mutex> lock(worker.mutex);
    return {worker.state,worker.remaining,worker.affinity,worker.affinityApplied,worker.started,bool(worker.failure),worker.stackSize,worker.priority,worker.priorityApplied};
}
const std::vector<size_t> &AvsNativeWorkerPool::initialStartOrder() const noexcept { return impl_->order; }
void AvsNativeWorkerPool::stop() { impl_->stop(); }
uintptr_t AvsNativeWorkerPool::currentWorkerIdentity() noexcept { return workerIdentity; }
bool AvsNativeWorkerPool::isCurrentWorker(size_t slot) const {
    return workerIdentity==reinterpret_cast<uintptr_t>(impl_->workers.at(slot).get());
}
} // namespace rb
