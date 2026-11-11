#include "rb/AvsDefaultFile.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <vector>
namespace {
void require(bool value) { if (!value) throw std::runtime_error("AVS default file assertion failed"); }
constexpr int32_t error(uint32_t code) { return int32_t(0x80070000u|code); }
struct Fixture {
    std::filesystem::path path;
    ~Fixture() { std::error_code ec; std::filesystem::remove(path,ec); }
};
}
int main(int argc,char **argv) { try {
    if (argc>2 && std::string(argv[1])=="--batch") {
        const auto path=std::filesystem::path(argv[2])/"avs-default-file-oracle-fixture.bin";
        require(!std::filesystem::exists(path)); Fixture fixture{path};
        std::string line;
        while(std::getline(std::cin,line)) {
            std::istringstream row(line);unsigned length,offset,count;row>>length>>offset>>count;
            std::string data;for(unsigned i=0;i<length;++i)data+=char(i%251);
            {std::ofstream f(path,std::ios::binary|std::ios::trunc);f.write(data.data(),data.size());require(f.good());}
            std::vector<std::vector<unsigned char>> storage;
            for(unsigned i=0;i<count;++i){unsigned size;row>>size;storage.emplace_back(size,0xcc);}
            rb::AvsDefaultFile f;auto host=path;require(f.open(host.make_preferred().string(),1,0444,false)==0);
            require(f.seek(offset,0)==offset);
            std::vector<rb::AvsReadBuffer> buffers;
            for(auto &b:storage)buffers.push_back({b.data(),uint32_t(b.size())});
            std::cout<<f.readv(buffers)<<' '<<f.seek(0,1);
            constexpr char digits[]="0123456789abcdef";
            for(const auto &b:storage){std::cout<<' ';if(b.empty())std::cout<<'-';for(auto c:b)std::cout<<digits[c>>4]<<digits[c&15];}
            std::cout<<'\n';
        }
        return 0;
    }
    // Caller controls the output directory; local analysis supplies an E: path.
    auto directory=argc>1 ? std::filesystem::path(argv[1]) : std::filesystem::absolute(argv[0]).parent_path();
    auto path=directory/"avs-default-file-test-fixture.bin";
    require(!std::filesystem::exists(path));
    Fixture fixture{path};
    const std::string payload="0123456789abcdefghijklmnopqrstuvwxyz";
    { std::ofstream file(path,std::ios::binary);file<<payload;require(file.good()); }
    rb::AvsDefaultFile file;
    std::string host=path.make_preferred().string();
    require(file.open(host,2,0444,false)==error(95));
    require(file.open(host,0,0444,false)==error(22));
    for (uint32_t flags : {0x5u, 0x9u, 0x11u, 0x21u, 0x81u, 0x101u,
                           0x31u, 0x91u, 0x80000001u, 0xffffffbdu}) {
        require(file.open(host,flags,0444,false)==0);
        require(file.seek(0,2)==int64_t(payload.size()));
        require(file.close()==0);
        const auto missing = host+".missing";
        require(file.open(missing,flags,0444,false)==error(2));
        require(!file.isOpen() && !std::filesystem::exists(missing));
    }
    require(file.open(host,1,0444,false)==0);
    require(file.open(host,1,0444,false)==error(16));
    char data[100]{};
    require(file.read(data,4)==4 && std::string(data,4)=="0123");
    require(file.seek(2,1)==6);
    require(file.read(data,3)==3 && std::string(data,3)=="678");
    require(file.seek(-2,2)==34);
    require(file.read(data,10)==2 && std::string(data,2)=="yz");
    require(file.read(data,10)==0);
    require(file.seek(0,3)==error(22));
    require(file.seek(0,0)==0);
    char a[3]{}, b[50]{}, c[4]{'!','!','!','!'};
    require(file.readv({{a,3},{b,50},{c,4}})==36);
    require(std::string(a,3)=="012" && std::string(b,33)==payload.substr(3));
    require(c[0]=='!' && c[3]=='!');
    require(file.readv({})==0);
    require(file.seek(0,0)==0);
    require(file.readv({{a,3},{nullptr,4}})==error(22));
    require(file.seek(0,1)==3); // Prior reads remain consumed after an error.
    rb::AvsFileStat info;
#if defined(_WIN32) || defined(__APPLE__)
    require(file.stat(info));
    require(info.size==payload.size() && info.type==2 && info.permissions==0644 && info.links>=1);
    require(info.modifiedMilliseconds>0 && info.createdMilliseconds>0);
#endif
    require(file.close()==0);
    require(file.open(host,0xfffffffdu,0444,false)==0);
    require(file.read(data,1)==0); // Ignored bits do not hide the active 0x40 bit.
    require(file.close()==0);
    require(file.read(data,1)==error(9));
    require(file.close()==error(9));
    require(!file.stat(info));
    require(file.open(host,0x41,0444,false)==0);
    require(file.read(data,1)==0); // Original append flag seeks EOF even read-only.
    require(file.close()==0);
    require(file.open(host+".missing",1,0444,false)==error(2));
    std::ifstream original(path,std::ios::binary);
    require(std::string(std::istreambuf_iterator<char>(original),{})==payload);
    std::cout<<"PASS AVS real host read-only open/read/readv/seek/stat/close and unchanged bytes\n";
} catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; } }
