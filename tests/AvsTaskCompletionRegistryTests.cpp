#include "rb/AvsTaskCompletionRegistry.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
using Registry=rb::AvsTaskCompletionRegistry;
Registry *current;
std::vector<std::pair<unsigned,uint32_t>> trace;
std::vector<std::pair<char,unsigned>> changes[9];
void invoke(unsigned,uint32_t);
template<unsigned N>void handler(uint32_t token){invoke(N,token);}
const Registry::Handler handlers[]={nullptr,handler<1>,handler<2>,handler<3>,handler<4>,handler<5>,handler<6>,handler<7>,handler<8>};
void invoke(unsigned id,uint32_t token) {
    trace.push_back({id,token});
    for(auto change:changes[id]) {
        if(change.first=='A')current->add(handlers[change.second]);
        else current->remove(handlers[change.second]);
    }
}
unsigned number(Registry::Handler value){for(unsigned i=0;i<9;++i)if(handlers[i]==value)return i;throw std::logic_error("unknown callback");}
void require(bool value){if(!value)throw std::runtime_error("AVS completion registry regression");}
}
int main(int argc,char **argv){try{
    Registry registry;current=&registry;
    if(argc>1&&std::string(argv[1])=="--batch") {
        char op;
        while(std::cin>>op) {
            if(op=='M'){unsigned id,target;char change;std::cin>>id>>change>>target;changes[id].push_back({change,target});std::cout<<"M\n";}
            else if(op=='Z'){for(auto &values:changes)values.clear();std::cout<<"Z\n";}
            else if(op=='D'){
                uint32_t token;std::cin>>token;trace.clear();registry.dispatch(token);std::cout<<"D";
                for(auto entry:trace)std::cout<<' '<<entry.first<<':'<<entry.second;std::cout<<'\n';
            } else {
                unsigned id;std::cin>>id;auto result=op=='A'?registry.add(handlers[id]):registry.remove(handlers[id]);
                std::cout<<uint32_t(result);for(auto value:registry.entries())std::cout<<' '<<number(value);std::cout<<'\n';
            }
        }return 0;
    }
    require(registry.add(nullptr)==0&&registry.remove(nullptr)==0);
    for(unsigned i=1;i<=4;++i)require(registry.add(handlers[i])==0);
    require(registry.add(handlers[5])==Registry::unavailable);
    require(registry.add(handlers[2])==0);
    require(registry.remove(handlers[1])==0&&registry.add(handlers[2])==0);
    require(registry.entries()[0]==handlers[2]&&registry.entries()[1]==handlers[2]);
    require(registry.remove(handlers[2])==0&&registry.entries()[0]==nullptr&&registry.entries()[1]==handlers[2]);
    require(registry.add(handlers[1])==0);
    changes[1]={{'R',2},{'A',5}};registry.dispatch(0x40003);
    require(trace==std::vector<std::pair<unsigned,uint32_t>>({{1,0x40003},{5,0x40003},{3,0x40003},{4,0x40003}}));
    require(registry.remove(handlers[2])==Registry::unavailable);
    std::cout<<"PASS original AVS completion registry holes, duplicates, nulls and live callback mutation\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
