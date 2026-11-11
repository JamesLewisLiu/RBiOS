#include "rb/AvsMountRegistry.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace rb;
namespace {
void require(bool value) { if (!value) throw std::runtime_error("AVS mount registry assertion failed"); }
constexpr int32_t error(uint32_t code) { return int32_t(0x80070000u|code); }
std::string hex(const std::string &s) { std::string out;constexpr char digits[]="0123456789abcdef";for(unsigned char c:s){out+=digits[c>>4];out+=digits[c&15];}return out.empty()?"-":out; }
std::string decode(const std::string &s) { std::string out;if(s=="-")return out;for(size_t i=0;i<s.size();i+=2)out+=char(std::stoul(s.substr(i,2),nullptr,16));return out; }
}
int main(int argc,char **argv) { try {
    if(argc>1 && std::string(argv[1])=="--batch") {
        std::string line;
        while(std::getline(std::cin,line)) {
            std::istringstream input(line);unsigned count;input>>count;
            AvsMountRegistry registry(128,128);registry.registerReadOnlyDefaultFilesystem();
            for(unsigned i=0;i<count;++i){std::string path,device;input>>path>>device;require(registry.mount({decode(path),decode(device),"fs",""})>0);}
            std::string query,host;input>>query;const auto status=registry.convertPath(decode(query),host);
            std::cout<<status<<' '<<hex(host)<<'\n';
        }
        return 0;
    }
    const auto base=(argc>1?std::filesystem::path(argv[1]):std::filesystem::absolute(argv[0]).parent_path())/"avs-mount-registry-fixture";
    require(!std::filesystem::exists(base));
    std::filesystem::create_directories(base/"root");std::filesystem::create_directories(base/"raw");
    struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}} cleanup{base};
    {std::ofstream f(base/"root"/"item.bin",std::ios::binary);f<<"root bytes";}
    {std::ofstream f(base/"raw"/"item.bin",std::ios::binary);f<<"raw bytes";}
    AvsMountRegistry registry;
    require(registry.mount({"/",(base/"root").string(),"fs",""})==error(19));
    registry.registerReadOnlyDefaultFilesystem();
    require(registry.mount({"/",(base/"root").string(),"fs","posix=0"})>0);
    require(registry.mount({"/dev/raw",(base/"raw").string(),"fs",""})>0);
    require(registry.mount({"/dev/nvram",(base/"raw").string(),"fs","sz_block=512,nr_block=32,nr_mirror=4"})>0);
    require(registry.mount({"/", "ignored", "fs", ""})==error(17));
    require(registry.mount({"/unsupported", "ignored", "nvram", ""})==error(19));
    require(registry.mount({"/options", "ignored", "fs", "posix=wrong"})==error(22));
    require(registry.mountedCount()==3);
    std::unique_ptr<AvsMountedFile> file;
    require(registry.open("/dev/raw/./item.bin",file)==0);
    char bytes[32]{};require(file->read(bytes,32)==9 && std::string(bytes,9)=="raw bytes");
    require(registry.unmount("/dev/raw")==error(16));
    require(file->close()==0);file.reset();
    require(registry.unmount("/dev/raw")==0);
    require(registry.unmount("/dev/raw")==error(19));
    require(registry.open("/item.bin",file)==0);
    require(file->read(bytes,32)==10 && std::string(bytes,10)=="root bytes");
    file.reset();
    require(registry.open("/missing.bin",file)==error(2) && !file);
    require(registry.unmount("/")==0); // Failed opens must release mount refs.
    require(registry.open("/item.bin",file)==error(19));
    require(registry.open("/dev/nvram/item.bin",file)==0);file.reset();
    require(registry.unmount("/dev/nvram")==0);
    require(registry.mountedCount()==0);
    // Mounting does not probe device existence, matching default-fs mount.
    require(registry.mount({"/",(base/"missing").string(),"fs",""})>0);
    require(registry.open("/item.bin",file)==error(2));
    require(registry.unmount("/")==0);
    AvsMountRegistry limited(8,1);limited.registerReadOnlyDefaultFilesystem();
    require(limited.mount({"/",(base/"root").string(),"fs",""})>0);
    require(limited.mount({"/raw",(base/"raw").string(),"fs",""})==error(12));
    require(limited.unmount("/")==0);
    require(limited.mount({"/raw",(base/"raw").string(),"fs",""})>0);
    {AvsMountRegistry temporary;temporary.registerReadOnlyDefaultFilesystem();require(temporary.mount({"/",(base/"root").string(),"fs",""})>0);require(temporary.open("/item.bin",file)==0);}
    require(file->read(bytes,32)==10);file.reset(); // Shared state outlives wrapper.
    std::cout<<"PASS AVS mount lookup, host reads, busy unmount, failed-open rollback and lifetime\n";
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;} }
