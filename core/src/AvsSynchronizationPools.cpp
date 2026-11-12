#include "rb/AvsSynchronizationPools.hpp"
namespace rb {
namespace {
const char *name(AvsSynchronizationPools::Kind kind) {
    switch(kind) {
    case AvsSynchronizationPools::Kind::condition:return "condition";
    case AvsSynchronizationPools::Kind::event:return "event";
    case AvsSynchronizationPools::Kind::semaphore:return "semaphore";
    case AvsSynchronizationPools::Kind::conditionState:return "condition-state";
    }
    return "unknown";
}
}
AvsSynchronizationPools::InitializationError::InitializationError(Kind stage,const std::string &reason)
    : std::runtime_error(std::string("AVS ")+name(stage)+" pool initialization failed: "+reason),stage_(stage) {}
AvsSynchronizationPools::AvsSynchronizationPools(AvsDescriptors &descriptors,const AvsBootConfigImage &config) {
    constexpr uint32_t sizes[]={20,24,4,20};
    for(size_t i=0;i<4;++i) {
        const auto kind=static_cast<Kind>(i);
        const auto offset=108+i*2;
        const uint16_t count=uint16_t(config[offset])|uint16_t(config[offset+1])<<8;
        try {
            pools_[i]=std::make_unique<AvsResourcePool>(descriptors,sizes[i],count);
            const auto bytes=AvsResourcePool::wrapperArenaBytes(sizes[i],count);
            state_[i]={kind,count,sizes[i],pools_[i]->descriptor(),arenaBytes_,bytes};
            arenaBytes_+=bytes;
        } catch(const std::exception &error) { throw InitializationError(kind,error.what()); }
    }
}
}
