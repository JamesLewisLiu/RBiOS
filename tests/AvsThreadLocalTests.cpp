#include "rb/AvsThreadLocal.hpp"
#include "rb/AvsThreadContext.hpp"
#include "rb/AvsTasks.hpp"
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value){if(!value)throw std::runtime_error("AVS shared TLS regression");}
struct Priority : rb::AvsThreadPriorityBackend {int32_t setCurrentPriority(int32_t)override{return 0;}};
struct TaskData {rb::AvsMutexes *mutexes;uint32_t mutex;uintptr_t mainIdentity;};
uint32_t completedToken=0;
void completed(uint32_t token) {
    require(rb::AvsThreadLocal::currentIdentity()!=0);
    require(rb::AvsThreadLocal::currentDescriptor()==token);completedToken=token;
}
int32_t inspect(uintptr_t value) {
    auto &data=*reinterpret_cast<TaskData *>(value);
    auto identity=rb::AvsThreadLocal::currentIdentity();
    require(identity&&identity!=data.mainIdentity);
    require(identity==rb::AvsThreadContext::currentIdentity());
    require(identity==rb::AvsNativeWorkerPool::currentWorkerIdentity());
    data.mutexes->lock(data.mutex,rb::AvsThreadContext::currentIdentity());data.mutexes->unlock(data.mutex);
    return int32_t(rb::AvsThreadLocal::currentDescriptor());
}
}
int main(int argc,char **argv){try{
    if(argc>1&&std::string(argv[1])=="--self") {
        int owner;const auto identity=reinterpret_cast<uintptr_t>(&owner);
        std::cout<<uint32_t(rb::AvsThreadLocal::self())<<'\n';
        rb::AvsThreadLocal::bind(identity);std::cout<<uint32_t(rb::AvsThreadLocal::self())<<'\n';
        for(uint32_t token:{0u,0x40003u,0x7f840004u}) {
            rb::AvsThreadLocal::setCurrentTask(identity,token,true);std::cout<<uint32_t(rb::AvsThreadLocal::self())<<'\n';
        }
        rb::AvsThreadLocal::unbind(identity);return 0;
    }
    require(!rb::AvsThreadLocal::currentIdentity()&&!rb::AvsThreadLocal::currentDescriptor());
    int owner;
    const auto identity=reinterpret_cast<uintptr_t>(&owner);
    rb::AvsThreadLocal::bind(identity,0x40003);
    require(rb::AvsThreadContext::currentIdentity()==identity&&rb::AvsThreadLocal::currentDescriptor()==0x40003);
    require(std::async(std::launch::async,[]{return rb::AvsThreadLocal::currentIdentity();}).get()==0);
    bool rejected=false;try{rb::AvsThreadLocal::bind(identity);}catch(const std::logic_error&){rejected=true;}require(rejected);
    rejected=false;try{rb::AvsThreadLocal::unbind(identity+1);}catch(const std::logic_error&){rejected=true;}require(rejected);
    require(rb::AvsThreadLocal::setCurrentDescriptor(identity,0)==0x40003);
    require(uint32_t(rb::AvsThreadLocal::self())==0x80010001u);
    rb::AvsThreadLocal::unbind(identity);
    rejected=false;try{rb::AvsThreadLocal::bind(0);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    {
        rb::AvsDescriptors descriptors(16);rb::AvsMutexes mutexes(descriptors,4);
        rb::AvsThreadBootstrapPlan plan;plan.threadCount=2;plan.initialCount=1;plan.expiryCount=0;
        plan.workerStack=65536;plan.minimumStack=4096;plan.maximumStack=131072;
        // Original 1001ABC0 constructs the task arena before the workers.
        rb::AvsResourcePool records(descriptors,rb::AvsTasks::recordSize(),plan.threadCount,rb::AvsTasks::recordAlignment());
        rb::AvsTaskCompletionRegistry completions;completions.add(completed);
        require((records.descriptor()&0x1ffff)==4);
        rb::AvsNativeWorkerPool workers(plan,mutexes);
        {
            rb::AvsTasks tasks(descriptors,workers,plan,records,&completions);
            Priority priority;rb::AvsThreadContext main(descriptors,mutexes,priority);main.start(2);
            const auto mainIdentity=rb::AvsThreadLocal::currentIdentity();
            require(rb::AvsThreadLocal::currentDescriptor()==main.descriptor());
            TaskData data{&mutexes,mutexes.create(),mainIdentity};
            const auto token=tasks.create(inspect,reinterpret_cast<uintptr_t>(&data),4096,2);
            require(token>0&&tasks.wait(token)==token&&completedToken==uint32_t(token));workers.waitIdle(1);
            require(rb::AvsThreadLocal::currentIdentity()==mainIdentity);
            require(rb::AvsThreadLocal::currentDescriptor()==main.descriptor());
            require(tasks.destroy(token)==0);mutexes.destroy(data.mutex);
            main.complete(0);main.stop();
        }
        require(records.freeCount()==plan.threadCount&&descriptors.lookup(records.descriptor()));
        require(!rb::AvsThreadLocal::currentIdentity());
        rb::AvsNativeWorkerPool privateWorker(rb::AvsNativeWorkerPool::PrivateOwner{},4096,0,mutexes);
        rejected=false;try{privateWorker.checkout(4096,0);}catch(const std::logic_error&){rejected=true;}require(rejected);
        auto result=privateWorker.assign(0,[]{
            require(rb::AvsThreadLocal::currentIdentity()!=0);
            return int32_t(rb::AvsThreadLocal::currentDescriptor());
        },{},0x40003);
        require(result.get()==0x40003);privateWorker.waitIdle(0);privateWorker.stop();
        require(!privateWorker.status(0).started&&privateWorker.status(0).affinity==-1);
    }
    std::cout<<"PASS unified AVS TLS, actual task descriptor, borrowed pool order and private worker\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
