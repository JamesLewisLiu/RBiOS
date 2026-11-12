#include "rb/AvsSynchronizationPools.hpp"
#include "rb/AvsMutexes.hpp"
#include <iostream>
#include <future>
#include <atomic>
#include <stdexcept>
namespace {
void require(bool value){if(!value)throw std::runtime_error("AVS synchronization pool regression");}
rb::AvsBootConfigImage config(){rb::AvsBootConfigImage image{};for(size_t i=0;i<4;++i)image[108+i*2]=uint8_t(i+1);return image;}
}
int main(int argc,char **argv){try {
    using Pools=rb::AvsSynchronizationPools;
    if(argc>1 && std::string(argv[1])=="--batch") {
        uint32_t counts[4];
        while(std::cin>>counts[0]>>counts[1]>>counts[2]>>counts[3]) {
            rb::AvsBootConfigImage image{};
            for(size_t i=0;i<4;++i){image[108+2*i]=uint8_t(counts[i]);image[109+2*i]=uint8_t(counts[i]>>8);}
            rb::AvsDescriptors descriptors(8);Pools pools(descriptors,image);
            for(const auto &s:pools.state())std::cout<<s.capacity<<' '<<s.elementSize<<' '<<s.descriptor<<' '<<s.originalArenaOffset<<' '<<s.originalArenaBytes<<' ';
            std::cout<<pools.originalArenaBytes()<<'\n';
        }
        return 0;
    }
    auto image=config();rb::AvsDescriptors descriptors(8);
    std::array<uint32_t,4> tokens;
    {
        Pools pools(descriptors,image);uint64_t offset=0;
        for(size_t i=0;i<4;++i) {
            const auto &s=pools.state()[i];tokens[i]=s.descriptor;
            require(s.kind==static_cast<Pools::Kind>(i) && s.capacity==i+1);
            require(s.descriptor==((uint32_t(i+1)<<23)|0x20000|uint32_t(i+4)));
            require(descriptors.lookup(s.descriptor) && s.originalArenaOffset==offset);
            require(s.originalArenaBytes==rb::AvsResourcePool::wrapperArenaBytes(s.elementSize,s.capacity));
            offset+=s.originalArenaBytes;
        }
        require(pools.originalArenaBytes()==offset);
        auto &sem=pools.semaphore();auto a=sem.allocate(),b=sem.allocate(),c=sem.allocate();
        require(a&&b&&c&&!sem.allocate()&&sem.statistics().active==3);
        sem.release(b);require(sem.allocate()==b);sem.release(a);sem.release(b);sem.release(c);
    }
    for(auto token:tokens)require(!descriptors.lookup(token));
    for(size_t fail=0;fail<4;++fail) {
        rb::AvsDescriptors d(4);auto broken=config();broken[108+fail*2]=0;bool rejected=false;
        try{Pools pools(d,broken);}catch(const Pools::InitializationError &error){rejected=error.stage()==static_cast<Pools::Kind>(fail);}
        require(rejected);
        // Every successfully constructed earlier pool must unwind its token.
        for(unsigned i=0;i<4;++i)require(d.allocate(7,nullptr,0)!=0);
        require(d.allocate(7,nullptr,0)==0);
    }
    for(unsigned capacity=1;capacity<4;++capacity) {
        rb::AvsDescriptors d(capacity);bool rejected=false;
        try{Pools pools(d,image);}catch(const Pools::InitializationError &error){rejected=error.stage()==static_cast<Pools::Kind>(capacity);}
        require(rejected);
        for(unsigned i=0;i<capacity;++i)require(d.allocate(7,nullptr,0)!=0);
    }
    // Registry exists for early worker TLS, but no mutex may be created until
    // the later original mutex-pool stage has allocated a real array.
    rb::AvsDescriptors d(8);
    auto mutexes=std::make_unique<rb::AvsMutexes>(d);
    require(!mutexes->resourcePoolBound() && !mutexes->create() && !mutexes->available());
    mutexes->registerCurrentThread(1);
    std::promise<void> registered,proceed;
    auto signal=proceed.get_future();
    auto worker=std::async(std::launch::async,[&]{mutexes->registerCurrentThread(2);registered.set_value();signal.wait();mutexes->unregisterCurrentThread();});
    registered.get_future().wait();
    rb::AvsResourcePool invalid(d,15,1);bool rejected=false;
    try{mutexes->bindResourcePool(invalid);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected && !mutexes->resourcePoolBound() && !mutexes->create());
    rb::AvsResourcePool pool(d,16,2);
    mutexes->bindResourcePool(pool);require(mutexes->resourcePoolBound() && mutexes->available()==2);
    auto token=mutexes->create();require(token && pool.statistics().active==1);
    mutexes->lock(token,1);mutexes->unlock(token);
    rejected=false;try{mutexes->bindResourcePool(pool);}catch(const std::logic_error&){rejected=true;}
    require(rejected && pool.statistics().active==1);
    proceed.set_value();worker.get();mutexes->unregisterCurrentThread();mutexes.reset();
    require(!d.lookup(token)&&d.lookup(pool.descriptor())&&pool.freeCount()==2);
    std::cout<<"PASS AVS passive synchronization pools, staged failure unwind and deferred mutex binding\n";
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}}
