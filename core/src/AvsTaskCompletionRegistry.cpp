#include "rb/AvsTaskCompletionRegistry.hpp"

namespace rb {
AvsTaskCompletionRegistry::AvsTaskCompletionRegistry() {
    for(auto &handler:handlers_)handler.store(nullptr,std::memory_order_relaxed);
}
int32_t AvsTaskCompletionRegistry::add(Handler callback) {
    std::lock_guard<std::mutex> lock(mutation_);
    for(auto &handler:handlers_) {
        const auto value=handler.load(std::memory_order_acquire);
        if(!value||value==callback) {
            handler.store(callback,std::memory_order_release);return 0;
        }
    }
    return unavailable;
}
int32_t AvsTaskCompletionRegistry::remove(Handler callback) {
    std::lock_guard<std::mutex> lock(mutation_);
    for(auto &handler:handlers_)if(handler.load(std::memory_order_acquire)==callback) {
        handler.store(nullptr,std::memory_order_release);return 0;
    }
    return unavailable;
}
void AvsTaskCompletionRegistry::dispatch(uint32_t descriptor) const {
    // The source reads each slot after the preceding callback returns. Taking
    // a snapshot or holding the mutation mutex would change callback mutations.
    for(const auto &handler:handlers_)if(const auto callback=handler.load(std::memory_order_acquire))callback(descriptor);
}
std::array<AvsTaskCompletionRegistry::Handler,4> AvsTaskCompletionRegistry::entries() const noexcept {
    std::array<Handler,4> result{};
    for(size_t i=0;i<result.size();++i)result[i]=handlers_[i].load(std::memory_order_acquire);
    return result;
}
} // namespace rb
