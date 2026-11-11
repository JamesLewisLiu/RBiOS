#include "rb/AvsTasks.hpp"
#include <new>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace rb {
struct AvsTasks::Record {
    AvsTasks *owner;
    uint32_t descriptor = 0;
    std::shared_future<int32_t> result;
    size_t worker = 0;
};
AvsTasks::AvsTasks(AvsDescriptors &descriptors, AvsNativeWorkerPool &workers, const AvsThreadBootstrapPlan &plan)
    : descriptors_(descriptors), workers_(workers), plan_(plan),
      pool_(descriptors, uint32_t(sizeof(Record)), plan.threadCount,
            uint32_t(alignof(Record)>8?alignof(Record):8)), bindings_(descriptors.capacity()+4) {}
AvsTasks::~AvsTasks() {
    // Host owner teardown waits for real actions; it does not fabricate task
    // completion or publish a successful original destroy for failed actions.
    for (;;) {
        Record *value;
        {std::lock_guard<std::recursive_mutex> lock(mutex_);if(records_.empty())break;value=records_.back();}
        if(value->result.valid())value->result.wait();
        descriptors_.replace(value->descriptor,nullptr,0);
        descriptors_.release(value->descriptor);
        releaseRecord(*value);
    }
    // Original bulk cleanup deliberately preserves lookup-visible tokens. Keep
    // that behavior while alive, but never leave a destroyed native owner in a
    // retained callback/payload. Index generations protect unrelated reuse.
    for(const auto &binding:bindings_) {
        uintptr_t payload=0;
        if(binding.first&&descriptors_.lookup(binding.first,&payload)&&payload==binding.second)
            descriptors_.replace(binding.first,nullptr,0);
    }
}
int32_t AvsTasks::create(Procedure procedure, uintptr_t argument, uint32_t stackSize, uint32_t priority) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    failure_=Failure::none;
    if(!procedure||!stackSize||(stackSize&4095)||priority>4){failure_=Failure::arguments;return invalidArgument;}
    if(((plan_.maximumStack-stackSize)|(stackSize-plan_.minimumStack))&0x80000000u)
        stackSize=stackSize>=plan_.minimumStack?plan_.maximumStack:plan_.minimumStack;
    void *memory=pool_.allocate();
    if(!memory){failure_=Failure::taskPool;return creationFailed;}
    Record *task=nullptr;
    try { task=new(memory)Record{this,0,{},0}; records_.push_back(task); }
    catch (...) {if(task)task->~Record();pool_.release(memory);failure_=Failure::control;return creationFailed;}
    std::optional<size_t> worker;
    try {worker=workers_.checkout(stackSize,priority,1);}
    catch (...) {releaseRecord(*task);failure_=Failure::worker;return creationFailed;}
    if(!worker){releaseRecord(*task);failure_=Failure::worker;return creationFailed;}
    task->worker=*worker;
    const auto token=descriptors_.allocate(2,dispose,reinterpret_cast<uintptr_t>(task));
    if(int32_t(token)<=0) {
        // 1001A7D0 does not put the checked-out worker back on the idle list.
        releaseRecord(*task);failure_=Failure::descriptor;return creationFailed;
    }
    task->descriptor=token;
    bindings_[token&0x1ffff]={token,reinterpret_cast<uintptr_t>(task)};
    try { task->result=workers_.assign(*worker,[procedure,argument]{return procedure(argument);}).share(); }
    catch (...) {
        descriptors_.replace(token,nullptr,0);descriptors_.release(token);
        releaseRecord(*task);failure_=Failure::dispatch;return creationFailed;
    }
    return int32_t(token);
}
AvsTasks::Record &AvsTasks::record(uint32_t token) {
    uintptr_t payload=0;
    if(((token>>17)&63)!=2||!descriptors_.lookup(token,&payload))throw std::invalid_argument("AVS task descriptor is invalid");
    for(auto *value:records_)if(reinterpret_cast<uintptr_t>(value)==payload&&value->descriptor==token)return *value;
    throw std::invalid_argument("AVS task belongs to another owner");
}
int32_t AvsTasks::wait(uint32_t token) {
    std::shared_future<int32_t> result;
    {std::lock_guard<std::recursive_mutex> lock(mutex_);auto &task=record(token);
      if(workers_.isCurrentWorker(task.worker)&&task.result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
          throw std::logic_error("AVS task cannot wait for itself");result=task.result;}
    return result.get();
}
int32_t AvsTasks::destroy(uint32_t token) {
    {std::lock_guard<std::recursive_mutex> lock(mutex_);auto &task=record(token);
      if(workers_.isCurrentWorker(task.worker)&&task.result.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
          throw std::logic_error("AVS task cannot destroy itself");}
    return descriptors_.release(token);
}
int32_t AvsTasks::dispose(uintptr_t payload) {
    auto &task=*reinterpret_cast<Record *>(payload);
    std::exception_ptr failure;
    try {task.result.get();}catch(...){failure=std::current_exception();}
    task.owner->releaseRecord(task);
    if(failure)std::rethrow_exception(failure);
    return 0;
}
void AvsTasks::releaseRecord(Record &task) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto found=std::find(records_.begin(),records_.end(),&task);
    if(found==records_.end())throw std::logic_error("AVS task control was already released");
    records_.erase(found);task.~Record();pool_.release(&task);
}
AvsTasks::Failure AvsTasks::lastFailure() const {std::lock_guard<std::recursive_mutex> lock(mutex_);return failure_;}
uint32_t AvsTasks::freeCount() const {return pool_.freeCount();}
uint32_t AvsTasks::poolDescriptor() const noexcept {return pool_.descriptor();}
} // namespace rb
