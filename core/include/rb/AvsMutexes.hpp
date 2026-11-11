#pragma once
#include "rb/AvsDescriptors.hpp"
#include <memory>
#include <mutex>
#include <vector>
#include <thread>
#include <map>

namespace rb {
class AvsResourcePool;
// Recovered AVS mutex resource/descriptor ownership. Owner must outlive every
// descriptor it creates. Destruction requires no holders or waiting threads.
class AvsMutexes {
public:
    AvsMutexes(AvsDescriptors &descriptors, uint16_t capacity);
    // The borrowed original 16-byte resource pool must share this descriptor
    // service and outlive this owner. Its capacity must fit uint16_t.
    // Do not reset or externally release this owner's allocated slots.
    AvsMutexes(AvsDescriptors &descriptors, AvsResourcePool &pool);
    ~AvsMutexes();
    AvsMutexes(const AvsMutexes &) = delete;
    AvsMutexes &operator=(const AvsMutexes &) = delete;
    uint32_t create();
    int32_t destroy(uint32_t token);
    uint32_t startDescriptorService();
    void stopDescriptorService();
    uint32_t serviceHandle() const noexcept { return service_; }
    // The original lock wrapper requires a registered thread TLS object.
    // A nonzero owner identity is supplied by that adapter, never fabricated.
    void lock(uint32_t token, uintptr_t registeredThreadIdentity);
    void unlock(uint32_t token);
    void registerCurrentThread(uintptr_t originalThreadIdentity);
    void checkCurrentThreadCanUnregister() const;
    // Native exception containment only. Releases locks held by the calling
    // thread; never changes another thread's ownership or reports task success.
    size_t releaseCurrentThreadLocksAfterFailure();
    void unregisterCurrentThread();
    size_t available() const;
private:
    struct Resource {
        Resource(AvsMutexes *owner, size_t index) : owner(owner), index(index) {}
        AvsMutexes *owner;
        std::shared_ptr<std::recursive_mutex> native;
        uint32_t token = 0;
        size_t users = 0;
        std::thread::id holder;
        size_t depth = 0;
        size_t index = 0;
        bool closing = false;
        void *poolSlot = nullptr;
    };
    static int32_t dispose(uintptr_t payload);
    Resource &resource(uint32_t token);
    AvsDescriptors &descriptors_;
    AvsResourcePool *externalPool_ = nullptr;
    mutable std::mutex poolMutex_;
    std::vector<std::unique_ptr<Resource>> records_;
    std::vector<uint32_t> ownedTokens_;
    std::vector<size_t> free_;
    std::map<std::thread::id, uintptr_t> registrations_;
    uint32_t service_ = 0;
};
} // namespace rb
