#include "rb/AvsPath.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
void require(bool ok) { if (!ok) throw std::runtime_error("AVS path assertion failed"); }
std::string decode(const std::string &s) {
    if (s == "-") return {};
    require(s.size() % 2 == 0); std::string out;
    for (size_t i=0;i<s.size();i+=2) out += char(std::stoul(s.substr(i,2), nullptr,16));
    return out;
}
std::string hex(const std::string &s) {
    std::string out; constexpr char digits[]="0123456789abcdef";
    for (unsigned char c:s) { out+=digits[c>>4]; out+=digits[c&15]; }
    return out.empty() ? "-" : out;
}
}
int main(int argc, char **argv) { try {
    if (argc>1 && std::string(argv[1])=="--batch") {
        unsigned capacity; std::string cwd,path;
        while (std::cin>>capacity>>cwd>>path) {
            auto r=rb::resolveAvsPath(decode(cwd),decode(path),capacity);
            std::cout<<r.status<<' '<<hex(r.path);
            for(auto hash:r.prefixHashes) std::cout<<' '<<hash;
            std::cout<<'\n';
        }
        return 0;
    }
    auto r=rb::resolveAvsPath("/data/music", "../graphics//./texture.ifs");
    require(r.path=="/data/graphics/texture.ifs" && (r.status>>16)==3 && r.prefixHashes.size()==4);
    require(rb::resolveAvsPath("/ignored", "/../../x").path=="/x");
    require(rb::resolveAvsPath("/base", "..").path=="/");
    require(rb::resolveAvsPath("", "").status==1);
    require(rb::resolveAvsPath("", "a").prefixHashes==std::vector<uint32_t>({0,97}));
    require(rb::resolveAvsPath("", "a/b").prefixHashes==std::vector<uint32_t>({0,97,3105}));
    require(rb::resolveAvsPath("", "a/b/../c").prefixHashes==std::vector<uint32_t>({0,97,3106}));
    require(rb::resolveAvsPath("", "a", 2).status==-2147024860);
    require(rb::resolveAvsPath("", "a", 3).path=="/a");
    require(rb::resolveAvsPath("", "a\\b").status==-2147024874);
    require(rb::resolveAvsPath("", std::string(1,char(128))).status==-2147024874);
    require(rb::resolveAvsPath("", std::string("a\0b",3)).path=="/a");
    std::string many; for(int i=0;i<64;++i) many+="/a";
    require(rb::resolveAvsPath("",many).status==((64<<16)|128));
    require(rb::resolveAvsPath("",many+"/b").status==-2147024860);
    require(rb::resolveAvsPath("",many+"/../b").status==((64<<16)|128));
    std::cout<<"PASS AVS path normalization, hash rollback, bounds and byte validation\n";
} catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; } }
