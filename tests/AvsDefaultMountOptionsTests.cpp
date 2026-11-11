#include "rb/AvsDefaultMountOptions.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char **argv) {try {
    if(argc>1 && std::string(argv[1])=="--batch") {
        std::string hex;
        while(std::cin>>hex) {
            std::string text;
            if(hex!="-")for(size_t i=0;i<hex.size();i+=2)text+=char(std::stoul(hex.substr(i,2),nullptr,16));
            rb::AvsDefaultMountOptions options;
            const auto status=rb::parseAvsDefaultMountOptions(text,options);
            std::cout<<status<<' '<<options.posix<<' '<<options.volumeFlush<<'\n';
        }
        return 0;
    }
    rb::AvsDefaultMountOptions options;
    auto check=[&](const char *text,int32_t status,bool posix,bool vf){
        if(rb::parseAvsDefaultMountOptions(text,options)!=status || options.posix!=posix || options.volumeFlush!=vf)
            throw std::runtime_error("AVS mount option mismatch");
    };
    check("",0,false,true);
    check("posix=0",0,false,true);
    check("posix=1",0,true,true);
    check("sz_block=512,nr_block=32,nr_mirror=4",0,false,true);
    check("vf=0,posix=0x100",0,true,false);
    check("posix=4294967296",0,false,true);
    check("posix=0x",0,false,true);
    check("posix=1,vf=\"0\"",0,true,false);
    check("posix=wrong",int32_t(0x80070016u),false,true);
    check("posix=1,vf=wrong",int32_t(0x80070016u),true,true);
    std::cout<<"PASS original default-fs option grammar and ignored nvram defaults\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
