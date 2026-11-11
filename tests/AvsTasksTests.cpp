#include "rb/AvsTasks.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
namespace {
void require(bool value){if(!value)throw std::runtime_error("AVS task admission regression");}
int32_t echo(uintptr_t value){return int32_t(value);}
int32_t fail(uintptr_t){throw std::runtime_error("task failure");}
struct Prior {rb::AvsTasks *tasks;uint32_t token;};
int32_t waitPrior(uintptr_t value){auto &prior=*reinterpret_cast<Prior *>(value);return prior.tasks->wait(prior.token)+1;}
struct ShutdownAdmission {rb::AvsTasks *tasks;std::promise<void> entered;std::future<void> gate;};
int32_t createDuringShutdown(uintptr_t value) {
    auto &request=*reinterpret_cast<ShutdownAdmission *>(value);
    request.entered.set_value();request.gate.wait();return request.tasks->create(echo,0,4096,2);
}
rb::AvsThreadBootstrapPlan plan(uint32_t count=2) {
    rb::AvsThreadBootstrapPlan p;p.threadCount=count;p.initialCount=count;p.minimumStack=4096;p.maximumStack=131072;
    p.workerStack=65536;p.expiryCount=0;p.affinityMask=3;return p;
}
}
int main(int argc,char **argv){try{
    if(argc>1&&std::string(argv[1])=="--failure") {
        rb::AvsDescriptors descriptors(1);rb::AvsMutexes mutexes(descriptors,1);auto p=plan(1);
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        for(int i=0;i<2;++i) {
            auto result=tasks.create(echo,0,4096,2);
            std::cout<<uint32_t(result)<<' '<<tasks.freeCount()<<' '<<workers.status(0).state<<'\n';
        }
        return 0;
    }
    {
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,2);auto p=plan(2);p.initialCount=1;
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        std::promise<void> release;ShutdownAdmission request{&tasks,{},release.get_future()};
        auto token=tasks.create(createDuringShutdown,reinterpret_cast<uintptr_t>(&request),4096,2);
        require(token>0);request.entered.get_future().wait();
        auto stopped=std::async(std::launch::async,[&]{workers.stop();});
        while(workers.status(1).state!=0)std::this_thread::yield();
        release.set_value();require(tasks.wait(token)==rb::AvsTasks::creationFailed);stopped.get();
        require(tasks.lastFailure()==rb::AvsTasks::Failure::worker&&tasks.freeCount()==1);
        require(tasks.destroy(token)==0&&tasks.freeCount()==2);
    }
    {
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,2);auto p=plan(2);
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        auto first=tasks.create(echo,10,4096,2);require(tasks.wait(first)==10);
        auto second=tasks.create(echo,20,4096,2);require(tasks.wait(second)==20);
        workers.waitIdle(0);workers.waitIdle(1);descriptors.replace(0x40003,nullptr,0);descriptors.cleanupType(2);
        auto next=tasks.create(echo,30,4096,2);require(tasks.wait(next)==30);
        bool rejected=false;try{tasks.wait(first);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
        require(tasks.destroy(next)==0);
    }
    {
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,1);auto p=plan(1);
        rb::AvsNativeWorkerPool workers(p,mutexes);uint32_t token=0;
        {
            rb::AvsTasks tasks(descriptors,workers,p);token=tasks.create(echo,7,4096,2);
            require(tasks.wait(token)==7);descriptors.replace(0x40003,nullptr,0);descriptors.cleanupType(2);
            uintptr_t payload=0;require(descriptors.lookup(token,&payload)&&payload!=0&&tasks.freeCount()==1);
        }
        uintptr_t payload=1;require(descriptors.lookup(token,&payload)&&payload==0);
    }
    {
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,1);auto p=plan(1);
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        for(uint32_t i=0;i<260;++i) {
            auto token=tasks.create(echo,i,4096,2);require(token>0&&tasks.wait(token)==int32_t(i));
            workers.waitIdle(0);require(tasks.destroy(token)==0);
        }
    }
    {
        // A recycled worker can wait on an older completed task on its slot.
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,2);auto p=plan(2);p.initialCount=1;
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        auto token=tasks.create(echo,99,4096,2);require(tasks.wait(token)==99);workers.waitIdle(1);
        Prior prior{&tasks,uint32_t(token)};
        auto next=tasks.create(waitPrior,reinterpret_cast<uintptr_t>(&prior),4096,2);
        require(next>0&&tasks.wait(next)==100);require(tasks.destroy(next)==0&&tasks.destroy(token)==0);
    }
    {
        rb::AvsDescriptors descriptors(16);rb::AvsMutexes mutexes(descriptors,4);auto p=plan();
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        require(tasks.create(nullptr,0,4096,2)==rb::AvsTasks::invalidArgument);
        require(tasks.create(echo,0,1,2)==rb::AvsTasks::invalidArgument);
        require(tasks.create(echo,0,4096,5)==rb::AvsTasks::invalidArgument);
        auto first=tasks.create(echo,12,4096,2);require(first>0&&tasks.wait(first)==12);
        auto second=tasks.create(echo,34,65536,0);require(second>0&&tasks.wait(second)==34);
        require(tasks.create(echo,0,4096,2)==rb::AvsTasks::creationFailed&&tasks.lastFailure()==rb::AvsTasks::Failure::taskPool);
        require(tasks.destroy(first)==0&&tasks.freeCount()==1);
        auto larger=tasks.create(echo,56,262144,2);require(larger>0&&tasks.wait(larger)==56);
        require(tasks.destroy(larger)==0&&tasks.destroy(second)==0&&tasks.freeCount()==2);
        auto failure=tasks.create(fail,0,65536,2);require(failure>0);
        bool rejected=false;try{tasks.wait(failure);}catch(const std::runtime_error&){rejected=true;}require(rejected);
        rejected=false;try{tasks.destroy(failure);}catch(const std::runtime_error&){rejected=true;}require(rejected&&tasks.freeCount()==2);
    }
    {
        // The array pool consumes the sole available descriptor. Original
        // export 091 checks out its worker before discovering this exhaustion.
        rb::AvsDescriptors descriptors(1);rb::AvsMutexes mutexes(descriptors,1);auto p=plan(1);
        rb::AvsNativeWorkerPool workers(p,mutexes);rb::AvsTasks tasks(descriptors,workers,p);
        require(tasks.create(echo,0,4096,2)==rb::AvsTasks::creationFailed);
        require(tasks.lastFailure()==rb::AvsTasks::Failure::descriptor&&tasks.freeCount()==1&&workers.status(0).state==2);
        require(tasks.create(echo,0,4096,2)==rb::AvsTasks::creationFailed&&tasks.lastFailure()==rb::AvsTasks::Failure::worker);
    }
    std::cout<<"PASS AVS finite task admission, stack clamp, descriptors, failures and checked-out side effects\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
