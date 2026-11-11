#include "rb/AvsResourcePool.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
namespace rb {
AvsResourcePool::Layout AvsResourcePool::layout(uintptr_t address,uint32_t size,uint32_t count,uint32_t alignment) {
    if(!address || (address&3) || !size || !count || alignment<8 || (alignment&(alignment-1)))
        throw std::invalid_argument("AVS resource pool layout arguments are invalid");
    count=std::min(count,0xfffffffdu);
    const uint64_t stride=(uint64_t(size)+8+alignment-1)&~uint64_t(alignment-1);
    if(stride>UINT32_MAX || address>UINTPTR_MAX-40-(alignment-1))
        throw std::overflow_error("AVS resource pool layout wraps the original arithmetic");
    const uint32_t padding=uint32_t((alignment-((address+40)&(alignment-1)))&(alignment-1));
    const uint64_t bytes=uint64_t(padding)+32+stride*count;
    return {count,uint32_t(stride),padding,padding+40,bytes};
}
uint64_t AvsResourcePool::wrapperArenaBytes(uint32_t size,uint32_t count) {
    if(!size || !count) throw std::invalid_argument("AVS resource pool count and element size must be nonzero");
    const uint64_t stride=(uint64_t(size)+15)&~uint64_t(7);
    if(stride>UINT32_MAX || uint64_t(count)>(UINT64_MAX-51)/stride)
        throw std::overflow_error("AVS resource pool wrapper stride or arena size overflows");
    return (uint64_t(count)*stride+51)&~uint64_t(15);
}
AvsResourcePool::AvsResourcePool(AvsDescriptors &descriptors,uint32_t size,uint32_t count,uint32_t alignment)
    : descriptors_(descriptors),elementSize_(size),count_(count),stride_(0),alignment_(alignment) {
    auto plan=layout(alignment,size,count,alignment);
    count_=plan.count;stride_=plan.stride;
    if(plan.arenaBytes>SIZE_MAX) throw std::overflow_error("AVS resource pool allocation is too large");
    slots_.resize(count_);
    arena_=static_cast<std::byte *>(::operator new(size_t(plan.arenaBytes),std::align_val_t(alignment_)));
    const auto actual=layout(reinterpret_cast<uintptr_t>(arena_),size,count_,alignment_);
    payload_=arena_+actual.firstPayloadOffset;
    initialize();
    // The original array owns a type-1 descriptor with a no-op callback; its
    // arena lifetime belongs to the enclosing resource owner.
    try { descriptor_=descriptors_.allocate(1,nullptr,reinterpret_cast<uintptr_t>(this)); }
    catch (...) { ::operator delete(arena_,std::align_val_t(alignment_));arena_=payload_=nullptr;throw; }
    if(int32_t(descriptor_)<=0) {
        // Match F900's signed failure check; supported descriptor tokens use
        // bits 0 through 30, so ordinary exhaustion reaches this branch as zero.
        if(descriptor_) {descriptors_.replace(descriptor_,nullptr,0);descriptors_.release(descriptor_);}
        ::operator delete(arena_,std::align_val_t(alignment_));arena_=payload_=nullptr;
        throw std::runtime_error("AVS resource pool descriptor allocation failed");
    }
}
AvsResourcePool::~AvsResourcePool() {
    uintptr_t payload=0;
    if(descriptors_.lookup(descriptor_,&payload) && payload==reinterpret_cast<uintptr_t>(this)) {
        descriptors_.replace(descriptor_,nullptr,0);
        descriptors_.release(descriptor_);
    }
    ::operator delete(arena_,std::align_val_t(alignment_));
}
bool AvsResourcePool::validDescriptor() const {
    uintptr_t payload=0;
    return descriptors_.lookup(descriptor_,&payload) && payload==reinterpret_cast<uintptr_t>(this);
}
void AvsResourcePool::initialize() {
    head_=tail_=cursor_=none;free_=0;available_=minimumFree_=count_;
    for(uint32_t i=0;i<count_;++i)slots_[i]={freeSlot,i+1<count_?i+1:none};
}
void *AvsResourcePool::pointer(uint32_t index) const { return index<count_?payload_+size_t(index)*stride_:nullptr; }
uint32_t AvsResourcePool::indexUnchecked(const void *p) const {
    const auto address=reinterpret_cast<uintptr_t>(p),start=reinterpret_cast<uintptr_t>(payload_);
    if(address<start)return none;
    const auto delta=address-start;
    return delta%stride_==0 && delta/stride_<count_?uint32_t(delta/stride_):none;
}
void *AvsResourcePool::allocate() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!validDescriptor() || free_==none)return nullptr;
    const uint32_t index=free_;auto &slot=slots_[index];free_=slot.next;
    if(head_==none)head_=index;
    if(tail_!=none)slots_[tail_].next=index;
    slot.previous=tail_;slot.next=none;tail_=index;
    --available_;minimumFree_=std::min(minimumFree_,available_);
    void *result=pointer(index);std::memset(result,0,payloadSize());return result;
}
uint32_t AvsResourcePool::release(void *p) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if(!validDescriptor())return 0; // Original invalid-array lookup returns zero.
    const auto index=indexUnchecked(p);
    if(index==none)throw std::invalid_argument("AVS resource pool release pointer is not a slot payload");
    auto &slot=slots_[index];
    if(slot.previous==freeSlot)throw std::logic_error("AVS resource pool slot is already free");
    if(slot.previous==none)head_=slot.next;else slots_[slot.previous].next=slot.next;
    if(slot.next==none)tail_=slot.previous;else slots_[slot.next].previous=slot.previous;
    slot.previous=freeSlot;slot.next=free_;free_=index;++available_;
    return index;
}
void AvsResourcePool::reset() {std::lock_guard<std::recursive_mutex> lock(mutex_);if(validDescriptor())initialize();}
uint32_t AvsResourcePool::freeCount() const {std::lock_guard<std::recursive_mutex> lock(mutex_);return available_;}
AvsResourcePool::Statistics AvsResourcePool::statistics() const {std::lock_guard<std::recursive_mutex> lock(mutex_);return {count_,count_-minimumFree_,count_-available_};}
uint32_t AvsResourcePool::indexOf(const void *p) const {std::lock_guard<std::recursive_mutex> lock(mutex_);return indexUnchecked(p);}
void *AvsResourcePool::at(uint32_t index,bool allocatedOnly) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return index<count_ && (!allocatedOnly || slots_[index].previous!=freeSlot)?pointer(index):nullptr;
}
bool AvsResourcePool::allocated(const void *p) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);auto index=indexUnchecked(p);return index!=none && slots_[index].previous!=freeSlot;
}
std::vector<uint32_t> AvsResourcePool::activeIndices() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);std::vector<uint32_t> result;
    for(auto index=head_;index!=none;index=slots_[index].next)result.push_back(index);return result;
}
void *AvsResourcePool::firstActive() {std::lock_guard<std::recursive_mutex> lock(mutex_);if(head_<count_)cursor_=slots_[head_].next;return pointer(head_);}
void *AvsResourcePool::lastActive() {std::lock_guard<std::recursive_mutex> lock(mutex_);if(tail_<count_)cursor_=slots_[tail_].previous;return pointer(tail_);}
void *AvsResourcePool::nextActive() {std::lock_guard<std::recursive_mutex> lock(mutex_);auto index=cursor_;if(index<count_)cursor_=slots_[index].next;return pointer(index);}
void *AvsResourcePool::previousActive() {std::lock_guard<std::recursive_mutex> lock(mutex_);auto index=cursor_;if(index<count_)cursor_=slots_[index].previous;return pointer(index);}
}
