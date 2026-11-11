#include "rb/CabinetConfigStore.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
namespace fs = std::filesystem;
using namespace rb;
void require(bool value) { if (!value) throw Error("Cabinet configuration assertion failed"); }
Bytes read(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(in), {});
}
void put(const fs::path &p, const Bytes &bytes) {
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
    require(bool(out));
}
template<class F> void rejects(F f) { bool caught = false; try { f(); } catch (const std::exception &) { caught = true; } require(caught); }
int main(int argc, char **argv) { try {
    (void)argc;
    const auto owned = fs::absolute(argv[0]).parent_path() /
        ("cabinet-config-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(fs::create_directory(owned));
    const auto root = owned / "resources", backups = owned / "backups";
    fs::create_directories(root / "prop"); fs::create_directories(root / "dev/nvram");
    const Bytes original{'<','x','>',0x82,0xa0,'<','/','x','>'};
    put(root / "prop/ea3-config.xml", original);
    CabinetConfigStore store(root, backups);
    store.replace("prop/ea3-config.xml", {'n','e','w'});
    require(read(backups / "prop/ea3-config.xml") == original);
    store.replace("prop/ea3-config.xml", {'l','a','s','t'});
    require(read(backups / "prop/ea3-config.xml") == original);
    require(read(root / "prop/ea3-config.xml") == Bytes({'l','a','s','t'}));
    store.replace("dev/nvram/clock.xml", {'c'});
    require(read(root / "dev/nvram/clock.xml") == Bytes({'c'}));
    for (const auto *path : {"data/a.xml", "prop/a.ifs", "prop/../a.xml", "prop/./a.xml", "dev/missing/a.xml"})
        rejects([&] { store.replace(path, {}); });
    rejects([&] { store.replace(root / "prop/ea3-config.xml", {}); });
    rejects([&] { CabinetConfigStore bad(root, root / "backups"); });
    put(owned / "blocked", {'b'});
    rejects([&] { CabinetConfigStore blocked(root, owned / "blocked/backups"); blocked.replace("prop/ea3-config.xml", {'x'}); });
    require(read(root / "prop/ea3-config.xml") == Bytes({'l','a','s','t'}));
    for (const auto &entry : fs::directory_iterator(root / "prop"))
        require(entry.path().filename() == "ea3-config.xml");
    std::cout << "PASS cabinet configuration raw-byte backup, replacement and write boundaries\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
