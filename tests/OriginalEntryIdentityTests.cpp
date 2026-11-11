#include "rb/OriginalEntryIdentity.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
using namespace rb;
template<size_t N> void unhex(const std::string&s,std::array<uint8_t,N>&out) {
    if(s.size()!=N*2) throw std::runtime_error("hex size");
    for(size_t i=0;i<N;++i)out[i]=uint8_t(std::stoul(s.substr(i*2,2),nullptr,16));
}
int main(int argc,char**){try {
    if(argc>1){std::string a,b;while(std::cin>>a>>b){OriginalEntryIdentity state;std::array<uint8_t,16>id;unhex(a,state.bytes);unhex(b,id);bool region=state.apply(id);for(auto c:state.bytes)std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(c);std::cout<<" "<<region<<"\n";}return 0;}
    OriginalEntryIdentity state;
    std::array<uint8_t,16> id{{'X','X','X','K','Z','Y','2','0','1','9','1','2','1','1','0','0'}};
    if(!state.apply(id)||state.bytes[0]!='M'||state.bytes[4]!='K'||state.bytes[6]!='A'||state.bytes[8]!='A'||state.bytes[13]!='9')throw std::runtime_error("identity semantics");
    const auto expected=state.bytes;id[3]=0;id[8]=0;
    if(state.apply(id)||state.bytes!=expected)throw std::runtime_error("empty field preservation");
    std::cout<<"PASS original entry identity mutation and preservation\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
