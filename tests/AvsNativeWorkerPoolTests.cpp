#include "rb/AvsNativeWorkerPool.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

namespace { void require(bool ok) { if (!ok) throw std::runtime_error("AVS native worker regression"); } }
int main(int argc,char **argv) { try {
    auto verifyCleanupFailure = [] {
        rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,3);
        auto token=mutexes.create();auto mainToken=mutexes.create();
        mutexes.registerCurrentThread(123);mutexes.lock(mainToken,123);
        rb::AvsThreadBootstrapPlan p;p.threadCount=1;p.initialCount=1;p.expiryCount=1;p.workerStack=65536;
        for(bool throws : {false,true}) {
            rb::AvsNativeWorkerPool pool(p,mutexes);
            auto task=pool.assign(0,[&]{
                auto identity=rb::AvsNativeWorkerPool::currentWorkerIdentity();
                mutexes.lock(token,identity);mutexes.lock(token,identity);
                if(throws)throw std::runtime_error("task failed while locked");return 1;
            });
            bool rejected=false;try{task.get();}catch(const std::exception&){rejected=true;}require(rejected);
            rejected=false;try{pool.waitIdle(0);}catch(const std::exception&){rejected=true;}require(rejected);
            pool.stop();require(pool.status(0).failed&&!pool.status(0).started);
            rejected=false;try{mutexes.checkCurrentThreadCanUnregister();}catch(const std::logic_error&){rejected=true;}require(rejected);
            mutexes.lock(token,123);mutexes.unlock(token); // Failed worker released only its own recursion.
        }
        mutexes.unlock(mainToken);mutexes.unregisterCurrentThread();
        mutexes.destroy(token);mutexes.destroy(mainToken);require(mutexes.available()==3);
    };
    if(argc>1&&std::string(argv[1])=="--cleanup-failure") {
        verifyCleanupFailure();std::cout<<"PASS worker failure containment releases only current-thread locks and preserves failure\n";return 0;
    }
    if(argc>1&&std::string(argv[1])=="--batch") {
        uint32_t count,initial,expiry;uint64_t mask;
        while(std::cin>>count>>initial>>expiry>>mask) {
            rb::AvsDescriptors descriptors(8);rb::AvsMutexes mutexes(descriptors,8);
            rb::AvsThreadBootstrapPlan p;p.threadCount=count;p.initialCount=initial;p.expiryCount=expiry;p.affinityMask=mask;p.workerStack=65536;
            rb::AvsNativeWorkerPool pool(p,mutexes);
            std::cout<<"I";for(auto slot:pool.initialStartOrder()){auto s=pool.status(slot);std::cout<<' '<<slot<<':'<<s.state<<':'<<s.remainingUses<<':'<<s.affinity;}std::cout<<'\n';
            for(auto slot:pool.initialStartOrder())for(int step=0;step<3&&pool.status(slot).state==1;++step) {
                std::string trace;rb::AvsNativeWorkerPool::Completion callbacks;
                for(int i=0;i<4;++i)callbacks[i]=[&,i]{trace+=char('0'+i);};
                auto result=pool.assign(slot,[step]{return 42+step;},std::move(callbacks));auto value=result.get();pool.waitIdle(slot);auto s=pool.status(slot);
                std::cout<<"T "<<slot<<' '<<value<<' '<<s.state<<' '<<s.remainingUses<<' '<<trace<<'\n';
            }
            pool.stop();std::cout<<"S\n";
        }return 0;
    }
    rb::AvsDescriptors descriptors(8); rb::AvsMutexes mutexes(descriptors, 8);
    const auto mutex = mutexes.create();
    rb::AvsThreadBootstrapPlan plan;
    plan.threadCount=3;plan.initialCount=2;plan.workerStack=65536;plan.expiryCount=2;plan.affinityMask=3;
    {
        rb::AvsNativeWorkerPool pool(plan,mutexes);
        require(pool.initialStartOrder()==std::vector<size_t>({2,1}));
        require(pool.status(0).state==0&&!pool.status(0).started);
        require(pool.status(2).state==1&&pool.status(2).affinity==0&&pool.status(1).affinity==1);
        require(rb::AvsNativeWorkerPool::currentWorkerIdentity()==0);
        std::atomic<int> executions{0};std::vector<int> callbacks;
        require(executions==0);
        rb::AvsNativeWorkerPool::Completion completion;
        for(int i=0;i<4;++i)completion[i]=[&,i]{callbacks.push_back(i);};
        auto result=pool.assign(2,[&]{
            ++executions;const auto identity=rb::AvsNativeWorkerPool::currentWorkerIdentity();require(identity!=0);
            mutexes.lock(mutex,identity);mutexes.unlock(mutex);return 42;
        },std::move(completion));
        require(result.get()==42);pool.waitIdle(2);
        require(callbacks==std::vector<int>({0,1,2,3})&&executions==1);
        require(pool.status(2).state==1&&pool.status(2).remainingUses==1);
        result=pool.assign(2,[]{return -7;});require(result.get()==-7);pool.waitIdle(2);
        require(pool.status(2).state==0&&pool.status(2).remainingUses==0);
        bool rejected=false;try{pool.assign(2,[]{return 0;});}catch(const std::logic_error&){rejected=true;}require(rejected);
        auto failed=pool.assign(1,[]{throw std::runtime_error("task failure");return 0;});
        rejected=false;try{failed.get();}catch(const std::runtime_error&){rejected=true;}require(rejected);
        rejected=false;try{pool.waitIdle(1);}catch(const std::runtime_error&){rejected=true;}require(rejected);
        pool.stop();require(!pool.status(1).started&&!pool.status(2).started);
    }
    {
        plan.initialCount=1;plan.expiryCount=0;
        rb::AvsNativeWorkerPool pool(plan,mutexes);
        for(int i=0;i<4;++i){require(pool.assign(2,[i]{return i;}).get()==i);pool.waitIdle(2);}
        require(pool.status(2).remainingUses==0&&pool.status(2).state==1);
        auto self=pool.assign(2,[&]{bool rejected=false;try{pool.stop();}catch(const std::logic_error&){rejected=true;}return rejected?1:0;});
        require(self.get()==1);pool.waitIdle(2);
        auto selfWait=pool.assign(2,[&]{bool rejected=false;try{pool.waitIdle(2);}catch(const std::logic_error&){rejected=true;}return rejected?1:0;});
        require(selfWait.get()==1);pool.waitIdle(2);
        std::promise<void> entered,release;auto gate=release.get_future();
        auto running=pool.assign(2,[&]{entered.set_value();gate.wait();return 99;});
        entered.get_future().wait();
        auto stopping=std::async(std::launch::async,[&]{pool.stop();});
        require(stopping.wait_for(std::chrono::milliseconds(10))==std::future_status::timeout);
        release.set_value();require(running.get()==99);stopping.get();
        require(!pool.status(2).started);
    }
    plan.workerPriority=5;bool rejected=false;
    try{rb::AvsNativeWorkerPool bad(plan,mutexes);}catch(const std::invalid_argument&){rejected=true;}require(rejected);
    mutexes.destroy(mutex);
    verifyCleanupFailure();
    std::cout<<"PASS native AVS worker creation, explicit dispatch, completion, expiry, failure and join\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
