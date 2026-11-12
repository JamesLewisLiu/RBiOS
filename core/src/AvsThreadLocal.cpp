#include "rb/AvsThreadLocal.hpp"
#include <stdexcept>

namespace rb {
namespace {
struct Binding { uintptr_t identity = 0; uint32_t descriptor = 0; bool attached = false; };
thread_local Binding binding;
void requireOwner(uintptr_t expected) {
    if (!expected || binding.identity != expected)
        throw std::logic_error("AVS TLS operation requires its actual bound native owner");
}
}
void AvsThreadLocal::bind(uintptr_t identity, uint32_t descriptor) {
    if (!identity) throw std::invalid_argument("AVS TLS identity cannot be zero");
    if (binding.identity) throw std::logic_error("AVS TLS already has a native owner");
    binding = {identity, descriptor, descriptor!=0};
}
void AvsThreadLocal::unbind(uintptr_t expectedIdentity) {
    requireOwner(expectedIdentity); binding = {};
}
uint32_t AvsThreadLocal::setCurrentDescriptor(uintptr_t expectedIdentity, uint32_t descriptor) {
    requireOwner(expectedIdentity);
    const auto previous=binding.descriptor;setCurrentTask(expectedIdentity,descriptor,descriptor!=0);return previous;
}
void AvsThreadLocal::setCurrentTask(uintptr_t expectedIdentity, uint32_t descriptor, bool attached) {
    requireOwner(expectedIdentity);binding.descriptor=descriptor;binding.attached=attached;
}
uintptr_t AvsThreadLocal::currentIdentity() noexcept { return binding.identity; }
uint32_t AvsThreadLocal::currentDescriptor() noexcept { return binding.descriptor; }
int32_t AvsThreadLocal::self() noexcept {
    return binding.identity&&binding.attached?int32_t(binding.descriptor):int32_t(0x80010001u);
}
} // namespace rb
