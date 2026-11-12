#include "rb/AvsReservedAlarmTask.hpp"
#include "rb/AvsNativeWorkerPool.hpp"
#include "rb/AvsThreadLocal.hpp"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <future>
#include <mutex>
#include <stdexcept>

namespace rb {
struct AvsReservedAlarmTask::Impl {
    AvsDescriptors &descriptors;
    AvsMutexes &mutexes;
    AvsAlarms &alarms;
    AvsReservedAlarmTask *owner;
    mutable std::mutex mutex;
    std::mutex lifecycle;
    std::condition_variable entered;
    std::unique_ptr<AvsNativeWorkerPool> worker;
    std::shared_future<int32_t> result;
    std::atomic<uintptr_t> identity{0};
    bool attempted = false, taskEntered = false, completed = false, joined = false, stopping = false;
    std::exception_ptr failure;
    int32_t value = 0;
    Impl(AvsDescriptors &d, AvsMutexes &m, AvsAlarms &a, AvsReservedAlarmTask *o)
        : descriptors(d), mutexes(m), alarms(a), owner(o) {}
    void rejectSelfWait() const {
        const auto active = identity.load();
        if (active && AvsThreadLocal::currentIdentity() == active)
            throw std::logic_error("AVS reserved alarm task cannot join itself");
    }
    void neutralize() {
        uintptr_t payload = 0;
        if (descriptors.lookup(descriptor, &payload) && payload == reinterpret_cast<uintptr_t>(this))
            descriptors.replace(descriptor, nullptr, 0);
    }
};
AvsReservedAlarmTask::AvsReservedAlarmTask(AvsDescriptors &d, AvsMutexes &m, AvsAlarms &a)
    : impl_(std::make_unique<Impl>(d,m,a,this)) {}
AvsReservedAlarmTask::~AvsReservedAlarmTask() {
    // Destruction on the active callback cannot preserve the worker's lifetime.
    // Owners must request shutdown there and destroy/join from another thread.
    try { impl_->rejectSelfWait(); } catch (...) { std::terminate(); }
    try { stop(); } catch (...) { /* Explicit stop()/wait() retain the failure. */ }
}
void AvsReservedAlarmTask::start() {
    std::lock_guard<std::mutex> lifecycle(impl_->lifecycle);
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->attempted) throw std::logic_error("AVS reserved alarm task cannot restart");
        if (impl_->stopping) throw std::logic_error("AVS reserved alarm task was cancelled before start");
        impl_->attempted = true;
    }
    try {
        uintptr_t payload = 0;
        if (!impl_->descriptors.lookup(descriptor, &payload) || payload)
            throw std::logic_error("AVS reserved alarm descriptor is unavailable or already owned");
        auto worker = std::make_unique<AvsNativeWorkerPool>(AvsNativeWorkerPool::PrivateOwner{},4096,0,impl_->mutexes);
        {
            std::lock_guard<std::mutex> lock(impl_->mutex); impl_->worker = std::move(worker);
        }
        if (!impl_->descriptors.replace(descriptor,dispose,reinterpret_cast<uintptr_t>(impl_.get())))
            throw std::logic_error("AVS reserved alarm descriptor binding failed");
        auto result = impl_->worker->assign(0,[this] {
            try {
                const auto identity = AvsThreadLocal::currentIdentity();
                if (!identity || AvsThreadLocal::currentDescriptor() != descriptor)
                    throw std::logic_error("AVS alarm worker has no reserved task TLS binding");
                impl_->identity = identity;
                {
                    std::lock_guard<std::mutex> lock(impl_->mutex); impl_->taskEntered = true;
                    impl_->entered.notify_all();
                }
                impl_->alarms.run();
                // Source 10040870 returns zero only after the stop loop exits.
                impl_->mutexes.checkCurrentThreadCanUnregister();
                {
                    std::lock_guard<std::mutex> lock(impl_->mutex); impl_->completed = true;
                    impl_->value = 0; impl_->entered.notify_all();
                }
                return 0;
            } catch (...) {
                std::lock_guard<std::mutex> lock(impl_->mutex);
                impl_->failure = std::current_exception(); impl_->entered.notify_all(); throw;
            }
        },{},descriptor).share();
        std::unique_lock<std::mutex> lock(impl_->mutex);
        impl_->result = std::move(result);
        impl_->entered.wait(lock,[&] { return impl_->taskEntered || impl_->failure; });
        if (impl_->failure) std::rethrow_exception(impl_->failure);
    } catch (...) {
        const auto failure = std::current_exception();
        impl_->alarms.requestStop();
        if (impl_->worker) impl_->worker->stop();
        impl_->neutralize();
        { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->failure = failure; impl_->joined = true; }
        std::rethrow_exception(failure);
    }
}
void AvsReservedAlarmTask::requestStop() {
    { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->stopping = true; }
    impl_->alarms.requestStop();
}
int32_t AvsReservedAlarmTask::wait() {
    impl_->rejectSelfWait();
    std::shared_future<int32_t> result;
    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        result = impl_->result;
        if (!result.valid()) {
            if (impl_->failure) std::rethrow_exception(impl_->failure);
            throw std::logic_error("AVS reserved alarm task has not been dispatched");
        }
    }
    return result.get();
}
void AvsReservedAlarmTask::stop() {
    impl_->rejectSelfWait();
    std::lock_guard<std::mutex> lifecycle(impl_->lifecycle);
    requestStop();
    std::exception_ptr failure;
    std::shared_future<int32_t> result;
    { std::lock_guard<std::mutex> lock(impl_->mutex); result = impl_->result; failure = impl_->failure; }
    if (result.valid()) try { result.get(); } catch (...) { failure = std::current_exception(); }
    if (impl_->worker && result.valid()) try { impl_->worker->waitIdle(0); } catch (...) { if (!failure) failure = std::current_exception(); }
    if (impl_->worker) try { impl_->worker->stop(); } catch (...) { if (!failure) failure = std::current_exception(); }
    impl_->neutralize();
    { std::lock_guard<std::mutex> lock(impl_->mutex); impl_->joined = true; if (failure) impl_->failure = failure; }
    if (failure) std::rethrow_exception(failure);
}
int32_t AvsReservedAlarmTask::dispose(uintptr_t payload) {
    // Original task destruction waits for actual completion; it does not turn
    // an arbitrary descriptor cleanup into an alarm stop request.
    auto &state = *reinterpret_cast<Impl *>(payload);
    state.owner->wait();
    return 0;
}
AvsReservedAlarmTask::Status AvsReservedAlarmTask::status() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    Status value{false,impl_->taskEntered,impl_->completed,impl_->joined,impl_->stopping,
                 bool(impl_->failure),impl_->value,impl_->identity.load(),4096,0,1,-1,false};
    if (impl_->worker) {
        const auto worker = impl_->worker->status(0);
        value.nativeStarted = worker.started; value.priorityApplied = worker.priorityApplied;
        value.remainingUses = worker.remainingUses; value.affinity = worker.affinity;
        value.failed = value.failed || worker.failed;
    }
    return value;
}
} // namespace rb
