#include "rb/AvsMutexes.hpp"
#include <stdexcept>
#include <system_error>

namespace rb {
AvsMutexes::AvsMutexes(AvsDescriptors &descriptors, uint16_t capacity) : descriptors_(descriptors) {
    if (!capacity) throw std::invalid_argument("AVS mutex pool count must be nonzero");
    records_.reserve(capacity);
    free_.reserve(capacity);
    for (uint32_t i = 0; i < capacity; ++i) {
        records_.push_back(std::make_unique<Resource>(Resource{this, {}}));
        records_.back()->index = i;
        free_.push_back(capacity - i - 1);
    }
}
AvsMutexes::~AvsMutexes() {
    // Native owner lifetime cleanup, separate from the original 023 operation.
    for (const auto &record : records_) if (record->native) dispose(reinterpret_cast<uintptr_t>(record.get()));
    for (auto token : ownedTokens_) {
        uintptr_t payload = 0;
        if (!descriptors_.lookup(token, &payload)) continue;
        for (const auto &record : records_) if (reinterpret_cast<uintptr_t>(record.get()) == payload) {
            descriptors_.replace(token, nullptr, 0);
            descriptors_.release(token);
            break;
        }
    }
}
uint32_t AvsMutexes::create() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    if (!free_.empty()) {
        try { ownedTokens_.reserve(ownedTokens_.size() + 1); }
        catch (const std::bad_alloc &) { return 0; }
        const auto &record = records_[free_.back()];
        try { record->native = std::make_shared<std::recursive_mutex>(); }
        catch (const std::system_error &) { return 0; }
        catch (const std::bad_alloc &) { return 0; }
        const auto token = descriptors_.allocate(3, dispose, reinterpret_cast<uintptr_t>(record.get()));
        if (!token) { record->native.reset(); return 0; }
        record->token = token;
        record->users = 0;
        record->depth = 0;
        record->closing = false;
        ownedTokens_.push_back(token);
        free_.pop_back();
        return token;
    }
    return 0;
}
int32_t AvsMutexes::dispose(uintptr_t payload) {
    auto &record = *reinterpret_cast<Resource *>(payload);
    std::lock_guard<std::mutex> lock(record.owner->poolMutex_);
    if (record.users) throw std::logic_error("AVS mutex cannot be destroyed with holders or waiters");
    record.closing = true;
    if (record.native) { record.native.reset(); record.owner->free_.push_back(record.index); }
    return 0;
}
AvsMutexes::Resource &AvsMutexes::resource(uint32_t token) {
    uintptr_t payload = 0;
    if (((token >> 17) & 63) != 3 || !descriptors_.lookup(token, &payload))
        throw std::invalid_argument("AVS mutex descriptor is invalid");
    for (const auto &record : records_)
        if (reinterpret_cast<uintptr_t>(record.get()) == payload && record->native && !record->closing) return *record;
    throw std::invalid_argument("AVS mutex resource is unavailable");
}
int32_t AvsMutexes::destroy(uint32_t token) {
    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        auto &record = resource(token);
        if (record.users) throw std::logic_error("AVS mutex cannot be destroyed with holders or waiters");
        // Reserve destruction before dropping the pool lock. A new lock caller
        // must not acquire this resource while descriptor release runs its
        // callback (which needs the same nonrecursive pool lock).
        record.closing = true;
    }
    return descriptors_.release(token);
}
uint32_t AvsMutexes::startDescriptorService() {
    if (service_) throw std::logic_error("AVS descriptor service already owns a mutex");
    service_ = create();
    if (!service_) throw std::runtime_error("AVS descriptor service cannot create mutex");
    return service_;
}
void AvsMutexes::stopDescriptorService() {
    if (!service_) throw std::logic_error("AVS descriptor service mutex is not started");
    uintptr_t payload = 0;
    if (!descriptors_.lookup(service_, &payload)) throw std::logic_error("AVS descriptor service mutex is invalid");
    dispose(payload); // Original 023 invokes the callback without descriptor release.
    service_ = 0;
}
void AvsMutexes::lock(uint32_t token, uintptr_t identity) {
    if (!identity) throw std::logic_error("AVS lock requires registered thread identity");
    std::shared_ptr<std::recursive_mutex> mutex;
    {
        std::lock_guard<std::mutex> lock(poolMutex_);
        const auto registered = registrations_.find(std::this_thread::get_id());
        if (registered == registrations_.end() || registered->second != identity)
            throw std::logic_error("AVS calling thread is not registered with that identity");
        auto &record = resource(token); mutex = record.native; ++record.users;
    }
    try { mutex->lock(); }
    catch (...) { std::lock_guard<std::mutex> lock(poolMutex_); --resource(token).users; throw; }
    { std::lock_guard<std::mutex> lock(poolMutex_); auto &record = resource(token);
      record.holder = std::this_thread::get_id(); ++record.depth; }
}
void AvsMutexes::unlock(uint32_t token) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    auto &record = resource(token);
    if (!record.depth || record.holder != std::this_thread::get_id())
        throw std::logic_error("AVS mutex unlock requires its owning thread");
    record.native->unlock();
    --record.depth;
    --record.users;
}
size_t AvsMutexes::available() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return free_.size();
}
void AvsMutexes::registerCurrentThread(uintptr_t identity) {
    if (!identity) throw std::invalid_argument("AVS thread identity must be nonzero");
    std::lock_guard<std::mutex> lock(poolMutex_);
    const auto current = std::this_thread::get_id();
    for (const auto &entry : registrations_)
        if (entry.first != current && entry.second == identity)
            throw std::logic_error("AVS thread identity is already registered to another thread");
    const auto found = registrations_.find(current);
    if (found != registrations_.end() && found->second != identity)
        throw std::logic_error("AVS current thread is already registered");
    registrations_[current] = identity;
}
void AvsMutexes::checkCurrentThreadCanUnregister() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    const auto current = std::this_thread::get_id();
    for (const auto &record : records_)
        if (record->depth && record->holder == current)
            throw std::logic_error("AVS thread still owns a mutex");
}
void AvsMutexes::unregisterCurrentThread() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    const auto current = std::this_thread::get_id();
    for (const auto &record : records_)
        if (record->depth && record->holder == current)
            throw std::logic_error("AVS thread still owns a mutex");
    registrations_.erase(current);
}
size_t AvsMutexes::releaseCurrentThreadLocksAfterFailure() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    const auto current = std::this_thread::get_id();
    size_t released = 0;
    for (const auto &record : records_) {
        if (record->holder != current) continue;
        while (record->depth) {
            record->native->unlock();
            --record->depth; --record->users; ++released;
        }
    }
    return released;
}
} // namespace rb
