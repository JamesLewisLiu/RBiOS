#include "rb/AvsThreadContext.hpp"
#include <stdexcept>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace rb {
thread_local AvsThreadContext *AvsThreadContext::current_ = nullptr;
int32_t AvsNativeThreadPriority::setCurrentPriority(int32_t priority) {
#if defined(_WIN32)
    return SetThreadPriority(GetCurrentThread(), priority) ? 0 : int32_t(0x80010016u);
#elif defined(__APPLE__) || defined(__linux__)
    // Normal scheduling has a direct native operation. Windows relative
    // priorities have no exact POSIX SCHED_OTHER equivalent, so reject them.
    if (priority != 0) return int32_t(0x80010016u);
    sched_param param{};
    return pthread_setschedparam(pthread_self(), SCHED_OTHER, &param) == 0 ? 0 : int32_t(0x80010016u);
#else
    (void)priority;
    return int32_t(0x80010016u);
#endif
}
int32_t AvsThreadContext::mapPriority(uint32_t priority) {
    if (priority > 4) throw std::invalid_argument("AVS main thread priority must be 0 through 4");
    return 2 - int32_t(priority);
}
AvsThreadContext::AvsThreadContext(AvsDescriptors &descriptors, AvsMutexes &mutexes,
    AvsThreadPriorityBackend &priorities) : descriptors_(descriptors), mutexes_(mutexes), priorities_(priorities) {}
AvsThreadContext::~AvsThreadContext() { abandonNativeOwner(); }
uintptr_t AvsThreadContext::currentIdentity() noexcept {
    return current_ ? reinterpret_cast<uintptr_t>(current_->identity_.get()) : 0;
}
void AvsThreadContext::start(uint32_t priority) {
    mapPriority(priority);
    if (current_ || control_) throw std::logic_error("AVS main thread context is already bound");
    thread_ = std::this_thread::get_id();
    control_ = std::make_unique<Control>();
    control_->owner = this;
    identity_ = std::make_unique<Identity>();
    // 100414C0 links the two records, marks state 3, and signals readiness.
    control_->identity = identity_.get();
    identity_->control = control_.get();
    identity_->state = 3;
    identity_->ready.signals = 1;
    identity_->ready.event.notify_one();
    current_ = this; // Native TLS analogue of 10039650, before descriptor allocation.
    descriptor_ = descriptors_.allocate(2, dispose, reinterpret_cast<uintptr_t>(control_.get()));
    ownedDescriptor_ = descriptor_;
    if (!descriptor_) throw std::runtime_error("AVS main thread descriptor allocation failed");
    control_->descriptor = descriptor_;
    control_->state = 0; // 10041160 binds descriptor and clears task fields.
    if (priority_ != priority) {
        priority_ = uint16_t(priority);
        if (priorities_.setCurrentPriority(mapPriority(priority)) < 0)
            throw std::runtime_error("AVS main thread priority is unsupported or failed");
    }
    mutexes_.registerCurrentThread(currentIdentity());
    registered_ = true;
}
int32_t AvsThreadContext::dispose(uintptr_t payload) {
    auto *control = reinterpret_cast<Control *>(payload);
    // 100410D0 waits for state 2. The main-thread owner must complete explicitly;
    // treating a still-running task as finished would fabricate its lifecycle.
    { std::unique_lock<std::recursive_mutex> lock(control->completion.mutex);
      if (control->state != 2)
          throw std::logic_error("AVS main thread has not completed its original lifecycle"); }
    control->owner->detach();
    return 0;
}
void AvsThreadContext::detach() {
    if (thread_ != std::this_thread::get_id())
        throw std::logic_error("AVS main thread context must detach on its owning thread");
    if (registered_) mutexes_.unregisterCurrentThread();
    registered_ = false;
    if (current_ == this) current_ = nullptr;
    descriptor_ = 0;
}
void AvsThreadContext::stop() {
    if (!control_) return;
    // Validate before descriptor release invalidates the token and enters the
    // destructor callback. Failed stop must preserve TLS and registration.
    if (thread_ != std::this_thread::get_id())
        throw std::logic_error("AVS main thread context must stop on its owning thread");
    if (registered_) mutexes_.checkCurrentThreadCanUnregister();
    { std::unique_lock<std::recursive_mutex> lock(control_->completion.mutex);
      if (control_->state != 2) throw std::logic_error("AVS main thread must complete before stop"); }
    if (ownedDescriptor_) {
        const auto token = ownedDescriptor_;
        uintptr_t payload = 0;
        if (descriptors_.lookup(token, &payload) && payload == reinterpret_cast<uintptr_t>(control_.get())) {
            descriptors_.release(token);
        // Bulk cleanup defers release and retains the old descriptor token.
            descriptors_.replace(token, nullptr, 0);
        }
    }
    if (current_ == this || registered_) detach();
    identity_.reset(); control_.reset(); priority_ = 0; ownedDescriptor_ = 0;
}
void AvsThreadContext::complete(int32_t result) {
    if (!registered_ || thread_ != std::this_thread::get_id())
        throw std::logic_error("AVS task completion requires its registered owner");
    std::lock_guard<std::recursive_mutex> lock(control_->completion.mutex);
    control_->result = result;
    control_->state = 2;
    control_->completion.event.notify_all();
}
void AvsThreadContext::abandonNativeOwner() {
    if (!control_) return;
    // Host RAII cleanup deliberately bypasses the original task destructor. It
    // neither signals completion nor reports a game-thread shutdown result.
    if (ownedDescriptor_) {
        uintptr_t payload = 0;
        if (descriptors_.lookup(ownedDescriptor_, &payload) && payload == reinterpret_cast<uintptr_t>(control_.get())) {
            descriptors_.replace(ownedDescriptor_, nullptr, 0);
            descriptors_.release(ownedDescriptor_);
        }
    }
    if (current_ == this || registered_) detach();
}
} // namespace rb
