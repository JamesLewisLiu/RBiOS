#include "rb/AvsResourcePool.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
using rb::AvsResourcePool;
static void require(bool value) {if(!value)throw std::runtime_error("resource pool assertion failed");}
static int batch() {
    rb::AvsDescriptors descriptors(32);
    std::unique_ptr<AvsResourcePool> pool;
    char op;uint32_t a,b,c;
    while(std::cin>>op>>a>>b>>c) {
        if(op=='I') {pool.reset();pool=std::make_unique<AvsResourcePool>(descriptors,a,b,c);std::cout<<pool->stride()<<' '<<pool->payloadSize()<<'\n';continue;}
        if(op=='L') {auto l=AvsResourcePool::layout(a,b,c,8);std::cout<<l.stride<<' '<<l.headerOffset<<' '<<l.firstPayloadOffset<<' '<<l.arenaBytes<<'\n';continue;}
        if(op=='A')std::cout<<pool->indexOf(pool->allocate());
        if(op=='R')std::cout<<pool->release(pool->at(a));
        if(op=='Z') {pool->reset();std::cout<<0;}
        if(op=='W') {std::memset(pool->at(a),b,pool->payloadSize());std::cout<<0;}
        if(op=='B') {auto p=static_cast<unsigned char *>(pool->at(a));unsigned sum=0;for(uint32_t i=0;i<pool->payloadSize();++i)sum+=p[i];std::cout<<sum;}
        if(op=='Q') {
            switch(a) {
            case 0:std::cout<<pool->statistics().active;break;
            case 1:std::cout<<pool->freeCount();break;
            case 2:{auto s=pool->statistics();std::cout<<s.capacity<<' '<<s.peakActive<<' '<<s.active;break;}
            case 3:std::cout<<pool->allocated(pool->at(b));break;
            case 4:std::cout<<pool->allocated(pool->at(b));break;
            case 6:std::cout<<pool->indexOf(pool->at(b));break;
            case 7:std::cout<<pool->indexOf(pool->at(b));break;
            case 8:std::cout<<pool->indexOf(pool->at(b,true));break;
            case 9:std::cout<<pool->indexOf(pool->firstActive());break;
            case 10:std::cout<<pool->indexOf(pool->lastActive());break;
            case 11:std::cout<<pool->indexOf(pool->nextActive());break;
            case 12:std::cout<<pool->indexOf(pool->previousActive());break;
            default:throw std::runtime_error("invalid batch query");
            }
        }
        std::cout<<'\n';
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc>1 && std::string(argv[1])=="--batch")return batch();
    rb::AvsDescriptors descriptors(16);
    uint32_t token;
    {
        AvsResourcePool pool(descriptors,17,3,32);token=pool.descriptor();
        require((token&0x7e0000)==0x20000 && descriptors.lookup(token));
        require(pool.stride()==32 && pool.payloadSize()==24);
        auto a=pool.allocate(),b=pool.allocate(),c=pool.allocate();
        require(a && b && c && !pool.allocate());
        require(reinterpret_cast<uintptr_t>(a)%32==0 && reinterpret_cast<uintptr_t>(b)%32==0);
        require(pool.activeIndices()==std::vector<uint32_t>({0,1,2}));
        std::memset(b,0xa5,pool.payloadSize());pool.release(b);pool.release(a);
        require(pool.allocate()==a && pool.allocate()==b);
        require(pool.activeIndices()==std::vector<uint32_t>({2,0,1}));
        auto p=static_cast<unsigned char *>(b);for(uint32_t i=0;i<pool.payloadSize();++i)require(!p[i]);
        require(pool.firstActive()==c && pool.nextActive()==a && pool.nextActive()==b && !pool.nextActive());
        require(pool.lastActive()==b && pool.previousActive()==a && pool.previousActive()==c && !pool.previousActive());
        require(!pool.allocated(static_cast<char *>(b)+1) && pool.indexOf(nullptr)==UINT32_MAX && !pool.at(3));
        bool threw=false;try{pool.release(static_cast<char *>(b)+1);}catch(const std::invalid_argument&){threw=true;}require(threw);
        pool.release(b);threw=false;try{pool.release(b);}catch(const std::logic_error&){threw=true;}require(threw);
        auto s=pool.statistics();require(s.capacity==3 && s.peakActive==3 && s.active==2);
        std::memset(a,0x42,pool.payloadSize());pool.reset();require(pool.freeCount()==3 && pool.statistics().peakActive==0);
        require(*static_cast<unsigned char *>(a)==0x42 && !pool.at(0,true));
        require(pool.allocate()==a && *static_cast<unsigned char *>(a)==0);
    }
    require(!descriptors.lookup(token));
    for(auto alignment:{8u,16u,64u,256u})for(auto offset:{0u,4u,8u,12u}) {
        auto l=AvsResourcePool::layout(0x1000+offset,13,7,alignment);
        require((0x1000+offset+l.firstPayloadOffset)%alignment==0);
        require(l.arenaBytes>=uint64_t(l.firstPayloadOffset)+6*l.stride+13);
    }
    bool threw=false;try{AvsResourcePool::layout(4,1,1,12);}catch(const std::invalid_argument&){threw=true;}require(threw);
    require(AvsResourcePool::wrapperArenaBytes(17,3)==144);
    threw=false;try{AvsResourcePool::wrapperArenaBytes(UINT32_MAX,UINT32_MAX);}catch(const std::overflow_error&){threw=true;}require(threw);
    threw=false;try{AvsResourcePool::layout(4,UINT32_MAX,1);}catch(const std::overflow_error&){threw=true;}require(threw);
    rb::AvsDescriptors exhausted(1);auto existing=exhausted.allocate(3,nullptr,0);
    threw=false;try{AvsResourcePool pool(exhausted,16,2);}catch(const std::runtime_error&){threw=true;}require(threw && exhausted.lookup(existing));
    // Real concurrent callers must never receive the same live slot.
    AvsResourcePool pool(descriptors,32,4,16);std::atomic<unsigned> owners[4]{};std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t)threads.emplace_back([&]{for(unsigned j=0;j<2000;++j){void *p;while(!(p=pool.allocate()))std::this_thread::yield();auto i=pool.indexOf(p);if(owners[i].fetch_add(1))failed=true;std::memset(p,0x73,32);if(owners[i].fetch_sub(1)!=1)failed=true;pool.release(p);}});
    for(auto &thread:threads)thread.join();require(!failed && pool.freeCount()==4 && pool.statistics().active==0);
    std::cout<<"PASS AVS resource pool ordering, queries, lifetime, alignment and contention\n";
}
