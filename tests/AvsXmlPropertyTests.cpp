#include "rb/AvsXmlProperty.hpp"
#include "rb/AvsConfigImport.hpp"
#include <iostream>
#include <stdexcept>
using namespace rb;
Bytes unhex(const std::string &s) { Bytes b; if (s != "-") for (size_t i = 0; i < s.size(); i += 2) b.push_back(uint8_t(std::stoul(s.substr(i, 2), nullptr, 16))); return b; }
std::string hex(const Bytes &b) { const char *d = "0123456789abcdef"; std::string s; for (auto v : b) { s += d[v >> 4]; s += d[v & 15]; } return s.empty() ? "-" : s; }
void dump(const Property &p) {
    std::cout << "N " << hex(Bytes(p.name.begin(), p.name.end())) << ' ' << unsigned(p.type) << ' ' << p.array << ' ' << hex(p.value) << '\n';
    for (const auto &[k, v] : p.attributes) std::cout << "A " << hex(Bytes(k.begin(), k.end())) << ' ' << hex(v) << '\n';
    for (const auto &c : p.children) dump(c);
    std::cout << "X\n";
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--tokens") {
        AvsXmlPropertyBuilder b(0x80, true); std::string op, name, value;
        while (std::cin >> op) {
            if (op == "B") {
                unsigned count; std::cin >> name >> count; const auto raw = unhex(name);
                std::vector<std::pair<std::string, Bytes>> attrs;
                for (unsigned i = 0; i < count; ++i) { std::cin >> name >> value; auto key = unhex(name); attrs.emplace_back(std::string(key.begin(), key.end()), unhex(value)); }
                b.begin(std::string(raw.begin(), raw.end()), attrs);
            } else if (op == "T") { std::cin >> value; b.text(unhex(value)); }
            else if (op == "E") { std::cin >> name; auto raw = unhex(name); b.end(std::string(raw.begin(), raw.end())); }
        }
        dump(b.finish().root); return 0;
    }
    AvsXmlPropertyBuilder b(0x80, true);
    b.begin("config", {}); b.begin("desc", {}); b.begin("nr_desc", {{"__type", {'u','1','6'}}}); b.text({'2','0'}); b.text({'4','8'}); b.end("nr_desc"); b.end("desc"); b.end("config");
    const auto document = b.finish(); const auto config = importAvsBootConfig(&document.root);
    if (config[68] != 0 || config[69] != 8) throw std::runtime_error("XML event to boot import");
    AvsXmlPropertyBuilder a(0x80, true); a.begin("v", {{"__type", {'u','1','6'}}, {"__count", {'3'}}}); a.text({'1',' ','2'}); a.end("v");
    const auto array = a.finish().root;
    if (!array.array || array.value != Bytes{0,1,0,2,0,0}) throw std::runtime_error("original array zero fill");
    std::cout << "PASS XML event conversion, split text, array zero fill and boot configuration import\n";
} catch (const std::exception &e) { if (argc > 1) { std::cout << "ERROR\n"; return 0; } std::cerr << e.what() << '\n'; return 1; } }
