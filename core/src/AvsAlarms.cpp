#include "rb/AvsAlarms.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <stdexcept>
namespace rb {
namespace {
constexpr int32_t invalid = -2147090423, argument = -2147090410, exhausted = -2147090420;
bool due(uint64_t deadline, uint64_t now) { return deadline && ((deadline - now - 1) >> 63); }
}
struct AvsAlarms::Impl {
    struct Record {
        Impl *owner; size_t index; Callback callback; uint64_t deadline = 0, period = 0;
        uint32_t token = 0; bool allocated = false, queued = false, retired = false, automatic = false;
    };
    AvsDescriptors &descriptors; Clock clock;
    mutable std::recursive_mutex mutex; std::mutex clockMutex;
    std::condition_variable_any wake;
    std::vector<std::unique_ptr<Record>> records;
    std::vector<size_t> free, queue;
    std::vector<uint32_t> tokens;
    uint32_t previousTick = 0, wraps = 0;
    uint64_t minimum = 0;
    uint32_t signals = 0, semaphoreLimit;
    size_t used = 0, highWater = 0;
    bool dispatching = false, pumping = false, worker = false, stopping = false;
    Impl(AvsDescriptors &d, uint16_t count, uint32_t limit, Clock c) : descriptors(d), clock(std::move(c)), semaphoreLimit(limit) {
        if (!count) throw std::invalid_argument("AVS alarm pool count must be nonzero");
        if (!limit || limit > 0x7fffffff) throw std::invalid_argument("AVS alarm semaphore limit must fit a positive LONG");
        if (!clock) clock = [] { return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); };
        records.reserve(count); free.reserve(count); queue.reserve(count);
        for (size_t i = 0; i < count; ++i) { records.push_back(std::make_unique<Record>(Record{this, i, {}})); free.push_back(i); }
    }
    uint64_t now() { std::lock_guard<std::mutex> lock(clockMutex); const auto tick = clock(); if (tick < previousTick) ++wraps; previousTick = tick; return (uint64_t(wraps) << 32) | tick; }
    Record *find(uint32_t token) {
        uintptr_t payload = 0;
        if ((token & 0x7e0000) != 0x100000 || !descriptors.lookup(token, &payload)) return nullptr;
        for (auto &r : records) if (reinterpret_cast<uintptr_t>(r.get()) == payload && r->allocated && r->token == token) return r.get();
        return nullptr;
    }
    Record *allocate(Callback callback, bool automatic) {
        if (free.empty()) return nullptr;
        auto &r = *records[free.back()]; free.pop_back(); ++used; highWater = std::max(used, highWater);
        r.callback = std::move(callback); r.deadline = r.period = 0; r.token = 0;
        r.allocated = true; r.queued = r.retired = false; r.automatic = automatic; return &r;
    }
    void release(Record &r) {
        if (dispatching) { r.retired = true; return; }
        if (r.queued) { queue.erase(std::find(queue.begin(), queue.end(), r.index)); r.queued = false; }
        r.allocated = false; r.callback = {}; --used; free.push_back(r.index);
    }
    void cleanup() {
        minimum = 0;
        for (auto it = queue.begin(); it != queue.end();) {
            auto &r = *records[*it];
            if (r.retired) { r.allocated = false; r.callback = {}; --used; free.push_back(r.index); it = queue.erase(it); }
            else if (!r.deadline) { r.queued = false; it = queue.erase(it); }
            else { if (!due(minimum, r.deadline)) minimum = r.deadline; ++it; }
        }
    }
    bool schedule(Record &r, uint64_t period, bool onlyStopped, uint64_t time) {
        const auto deadline = time + period;
        if (!r.queued) { r.queued = true; queue.insert(queue.begin(), r.index); }
        const bool active = r.deadline != 0;
        if (!(active && onlyStopped)) {
            r.deadline = deadline; r.period = period;
            if (!dispatching && !due(minimum, deadline)) {
                minimum = deadline;
                if (signals < semaphoreLimit) ++signals;
                wake.notify_all();
            }
        }
        return active;
    }
    size_t dispatch(uint64_t time, std::unique_lock<std::recursive_mutex> &lock) {
        std::vector<size_t> ready;
        for (auto index : queue) {
            auto &r = *records[index];
            if (!due(r.deadline, time)) continue;
            if (r.automatic) r.retired = true; else r.deadline = time + r.period;
            ready.push_back(index);
        }
        dispatching = true; size_t count = 0;
        for (auto it = ready.rbegin(); it != ready.rend() && !stopping; ++it) {
            auto &r = *records[*it]; auto callback = r.callback;
            lock.unlock(); bool keep;
            try { keep = callback(time); }
            catch (...) { lock.lock(); r.deadline = 0; stopping = true; wake.notify_all(); throw; }
            lock.lock(); if (!keep) r.deadline = 0; ++count;
        }
        return count;
    }
};
AvsAlarms::AvsAlarms(AvsDescriptors &d, uint16_t n, uint32_t limit, Clock clock) : impl_(new Impl(d, n, limit, std::move(clock))) {}
AvsAlarms::~AvsAlarms() {
    requestStop();
    std::unique_lock<std::recursive_mutex> lock(impl_->mutex);
    impl_->wake.wait(lock, [&] { return !impl_->worker && !impl_->pumping; });
    impl_->dispatching = false;
    for (auto token : impl_->tokens) {
        uintptr_t payload = 0;
        if (!impl_->descriptors.lookup(token, &payload)) continue;
        const bool owned = std::any_of(impl_->records.begin(), impl_->records.end(),
            [payload](const auto &record) { return reinterpret_cast<uintptr_t>(record.get()) == payload; });
        if (!owned) continue;
        // Bulk cleanup preserves the descriptor token even after its record is
        // reclaimed or reused. Remove every surviving owner binding, without
        // invoking dispose a second time on a potentially reused record.
        impl_->descriptors.replace(token, nullptr, 0);
        lock.unlock(); impl_->descriptors.release(token); lock.lock();
    }
}
int32_t AvsAlarms::dispose(uintptr_t payload) {
    auto &r = *reinterpret_cast<Impl::Record *>(payload); std::lock_guard<std::recursive_mutex> lock(r.owner->mutex);
    r.owner->release(r); return 0;
}
int32_t AvsAlarms::create(Callback callback) {
    if (!callback) return argument;
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    impl_->tokens.reserve(impl_->tokens.size() + 1);
    auto *r = impl_->allocate(std::move(callback), false); if (!r) return exhausted;
    const auto token = impl_->descriptors.allocate(8, dispose, reinterpret_cast<uintptr_t>(r));
    if (!token) { impl_->release(*r); return exhausted; }
    r->token = token; impl_->tokens.push_back(token); return int32_t(token);
}
int32_t AvsAlarms::destroy(uint32_t token) {
    { std::lock_guard<std::recursive_mutex> lock(impl_->mutex); if (!impl_->find(token)) return invalid; }
    return impl_->descriptors.release(token);
}
int32_t AvsAlarms::set(uint32_t token, uint64_t period) {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex); auto *r = impl_->find(token); if (!r) return invalid;
    if (!period) { const bool active = r->deadline != 0; r->deadline = 0; return active; }
    return impl_->schedule(*r, period, false, impl_->now());
}
int32_t AvsAlarms::setOnce(uint32_t token, uint64_t period) {
    if (!period) return argument;
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    auto *r = impl_->find(token); return r ? impl_->schedule(*r, period, true, impl_->now()) : invalid;
}
bool AvsAlarms::createProcedure(uint64_t period, Callback callback) {
    if (!period || !callback) return false;
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    auto *r = impl_->allocate(std::move(callback), true); if (!r) return false;
    impl_->schedule(*r, period, false, impl_->now()); return true;
}
size_t AvsAlarms::poll() {
    std::unique_lock<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->worker || impl_->pumping) throw std::logic_error("AVS alarm dispatch already owned");
    if (impl_->stopping) return 0;
    impl_->pumping = true; impl_->dispatching = false; impl_->cleanup();
    try { const auto count = impl_->dispatch(impl_->now(), lock); impl_->pumping = false; impl_->wake.notify_all(); return count; }
    catch (...) { impl_->pumping = false; impl_->wake.notify_all(); throw; }
}
void AvsAlarms::run() {
    std::unique_lock<std::recursive_mutex> lock(impl_->mutex);
    if (impl_->worker || impl_->pumping) throw std::logic_error("AVS alarm dispatch already owned");
    impl_->worker = true;
    try {
        while (!impl_->stopping) {
            impl_->dispatching = false; impl_->cleanup();
            const auto time = impl_->minimum ? impl_->now() : 0;
            if (!impl_->minimum || time < impl_->minimum) {
                const uint32_t delay = impl_->minimum ? uint32_t(impl_->minimum - time) : UINT32_MAX;
                auto predicate = [&] { return impl_->stopping || impl_->signals != 0; };
                if (delay == UINT32_MAX) impl_->wake.wait(lock, predicate);
                else impl_->wake.wait_for(lock, std::chrono::milliseconds(delay), predicate);
                if (impl_->signals) --impl_->signals;
            }
            if (!impl_->stopping) impl_->dispatch(impl_->now(), lock);
        }
        impl_->worker = false; impl_->wake.notify_all();
    } catch (...) { impl_->worker = false; impl_->wake.notify_all(); throw; }
}
void AvsAlarms::requestStop() {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex);
    if (!impl_->stopping && impl_->signals < impl_->semaphoreLimit) ++impl_->signals;
    impl_->stopping = true; impl_->wake.notify_all();
}
bool AvsAlarms::running() const { std::lock_guard<std::recursive_mutex> lock(impl_->mutex); return impl_->worker; }
AvsAlarms::State AvsAlarms::state() const {
    std::lock_guard<std::recursive_mutex> lock(impl_->mutex); State result{{},impl_->queue,impl_->free,impl_->minimum,impl_->used,impl_->highWater,impl_->signals};
    for (const auto &r : impl_->records) result.records.push_back({r->deadline,r->period,r->allocated,r->queued,r->retired,r->automatic});
    return result;
}
} // namespace rb
