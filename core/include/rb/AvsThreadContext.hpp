#pragma once
#include "rb/AvsDescriptors.hpp"
#include "rb/AvsMutexes.hpp"
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace rb {
class AvsThreadPriorityBackend {
public:
    virtual ~AvsThreadPriorityBackend() = default;
    virtual int32_t setCurrentPriority(int32_t windowsPriority) = 0;
};
class AvsNativeThreadPriority final : public AvsThreadPriorityBackend {
public:
    int32_t setCurrentPriority(int32_t windowsPriority) override;
};

// Registration of the calling main thread, not worker creation or the complete
// 1001ABC0 thread-pool bootstrap. Native TLS is bound only by explicit start.
class AvsThreadContext {
public:
    AvsThreadContext(AvsDescriptors &, AvsMutexes &, AvsThreadPriorityBackend &);
    ~AvsThreadContext();
    AvsThreadContext(const AvsThreadContext &) = delete;
    AvsThreadContext &operator=(const AvsThreadContext &) = delete;
    void start(uint32_t originalPriority);
    void stop();
    void complete(int32_t result);
    uint32_t descriptor() const noexcept { return descriptor_; }
    uint16_t priority() const noexcept { return priority_; }
    bool started() const noexcept { return registered_; }
    static uintptr_t currentIdentity() noexcept;
    static int32_t mapPriority(uint32_t originalPriority);
private:
    struct Channel {
        std::recursive_mutex mutex;
        std::condition_variable_any event;
        uint32_t signals = 0;
    };
    struct Identity;
    struct Control {
        AvsThreadContext *owner = nullptr;
        Identity *identity = nullptr;
        Channel completion;
        uint32_t descriptor = 0, state = 0;
        int32_t result = 0;
    };
    struct Identity {
        Control *control = nullptr;
        Channel ready, wake;
        uint16_t state = 0;
    };
    static int32_t dispose(uintptr_t payload);
    void detach();
    void abandonNativeOwner();
    AvsDescriptors &descriptors_;
    AvsMutexes &mutexes_;
    AvsThreadPriorityBackend &priorities_;
    std::unique_ptr<Control> control_;
    std::unique_ptr<Identity> identity_;
    std::thread::id thread_;
    uint32_t descriptor_ = 0;
    uint32_t ownedDescriptor_ = 0;
    uint16_t priority_ = 0;
    bool registered_ = false;
    static thread_local AvsThreadContext *current_;
};
} // namespace rb
