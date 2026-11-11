#include "rb/AvsThreadBootstrap.hpp"
#include "rb/AvsConfigImport.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("AVS thread bootstrap regression"); }
void put(rb::AvsBootConfigImage &image, size_t offset, uint32_t value, size_t bytes=4) {
    for(size_t i=0;i<bytes;++i) image[offset+i]=uint8_t(value>>(8*i));
}
}
int main(int argc, char **argv) { try {
    if (argc>1 && std::string(argv[1])=="--batch") {
        uint32_t mask,count,initial,minimum,maximum,stack,expiry,low,high;
        while(std::cin>>mask>>count>>initial>>minimum>>maximum>>stack>>expiry>>low>>high) {
            rb::AvsBootConfigImage image{};
            put(image,104,count,2);put(image,88,initial,2);put(image,96,minimum);put(image,100,maximum);
            put(image,80,stack);put(image,84,expiry);put(image,72,low);put(image,76,high);
            try {
                auto p=rb::planAvsThreadBootstrap(image,mask);
                std::cout<<p.processorCount<<' '<<p.spinCount<<' '<<p.semaphoreLimit<<' '<<p.minimumStack<<' '
                    <<p.maximumStack<<' '<<uint32_t(p.affinityMask)<<' '<<uint32_t(p.affinityMask>>32)<<' '
                    <<p.threadCount<<' '<<p.initialCount<<' '<<p.expiryCount<<' '<<p.workerStack<<' '
                    <<p.workerPriority<<' '<<p.taskArenaBytes<<' '<<p.workerArenaSuppliedBytes<<' '
                    <<p.workerArenaConsumedBytes;
                for(auto warning:p.warnings)std::cout<<' '<<int(warning);
                std::cout<<'\n';
            } catch(const std::invalid_argument&){std::cout<<"error\n";}
        }return 0;
    }
    if(argc>1 && std::string(argv[1])=="--affinity") {
        uint64_t mask;unsigned count;
        while(std::cin>>mask>>count){rb::AvsThreadAffinity a(mask);for(unsigned i=0;i<count;++i){if(i)std::cout<<' ';std::cout<<a.next();}std::cout<<'\n';}return 0;
    }
    auto image=rb::importAvsBootConfig();auto p=rb::planAvsThreadBootstrap(image,0xff);
    require(p.threadCount==32&&p.initialCount==8&&p.workerStack==65536&&p.affinityMask==255);
    require(p.spinCount==4000&&p.semaphoreLimit==32&&p.warnings.empty());
    put(image,104,2,2);put(image,88,4,2);put(image,84,1);put(image,80,1);
    p=rb::planAvsThreadBootstrap(image,0x80000000);
    require(p.initialCount==2&&p.spinCount==0&&p.affinityMask==1);
    require(p.warnings.size()==2&&p.workerStack==4096);
    put(image,80,0);p=rb::planAvsThreadBootstrap(image,0);require(p.workerStack==0&&p.affinityMask==0);
    rb::AvsThreadAffinity affinity(0x8000000000000085ull);
    for(int i=0;i<8;++i)require(affinity.next()==std::array<int,4>{0,2,7,63}[i%4]);
    std::cout<<"PASS AVS thread bootstrap policy and affinity selection\n";
} catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
