#include "rb/AvsConfigImport.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace rb;
namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
Property &insert(Property &root, const std::string &path) {
    auto *node = &root;
    size_t start = 0;
    for (;;) {
        auto end = path.find('/', start);
        const auto name = path.substr(start, end == std::string::npos ? end : end - start);
        auto next = std::find_if(node->children.begin(), node->children.end(), [&](const auto &p) { return p.name == name; });
        if (next == node->children.end()) { Property p; p.name = name; node->children.push_back(p); next = node->children.end() - 1; }
        node = &*next;
        if (end == std::string::npos) return *node;
        start = end + 1;
    }
}
void emit(const AvsBootConfigImage &image) {
    constexpr char digits[] = "0123456789abcdef";
    for (auto b : image) std::cout << digits[b >> 4] << digits[b & 15];
    std::cout << '\n';
}
} // namespace
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        unsigned count;
        while (std::cin >> count) {
            Property root; root.name = "config";
            for (unsigned i = 0; i < count; ++i) {
                std::string path, hex; unsigned type;
                std::cin >> path >> type >> hex;
                auto &node = insert(root, path); node.type = uint8_t(type & 63); node.array = (type & 64) != 0;
                if (hex != "-") for (size_t j = 0; j < hex.size(); j += 2) node.value.push_back(uint8_t(std::stoul(hex.substr(j, 2), nullptr, 16)));
            }
            emit(importAvsBootConfig(&root));
        }
        return 0;
    }
    Property root; root.name = "config";
    const auto defaults = importAvsBootConfig();
    require(defaults == importAvsBootConfig(&root), "null and empty config differ");
    require(defaults[68] == 0x28 && defaults[69] == 3, "original descriptor default is 808");
    for (size_t i = 72; i < 80; ++i) require(defaults[i] == 255, "64-bit pointed default");
    for (auto i : {67, 70, 71, 90, 124, 1987, 2001, 2258, 2259}) require(defaults[size_t(i)] == 0, "original padding must be zero");
    auto &count = insert(root, "fs/nr_mountpoint"); count.type = 5; count.value = {1, 244};
    auto &option = insert(root, "fs/root/option"); option.type = 11; option.value = {'p','o','s','i','x','=','0',0};
    const auto configured = importAvsBootConfig(&root);
    require(configured[130] == 244 && configured[131] == 1, "host-endian scalar import");
    require(configured[418] == '0', "original root option override");
    auto expectFailure = [&](const Property &p) {
        bool failed = false; try { (void)importAvsBootConfig(&p); } catch (const Error &) { failed = true; }
        require(failed, "unsupported property accepted");
    };
    auto invalid = root; invalid.name = "other"; expectFailure(invalid);
    invalid = root; insert(invalid, "fs/nr_mountpoint").array = true;
    require(importAvsBootConfig(&invalid) == configured, "original ignores array flag when byte count fits");
    invalid = root; insert(invalid, "fs/nr_mountpoint").type = 7; expectFailure(invalid);
    invalid = root; insert(invalid, "fs/nr_mountpoint").value.pop_back(); expectFailure(invalid);
    invalid = root; insert(invalid, "fs/root/option").value = Bytes(258, 'a'); expectFailure(invalid);
    invalid = root; insert(invalid, "fs/root/option").value = {'a', 0, 'b', 0};
    require(importAvsBootConfig(&invalid)[414] == 'b', "original copies string bytes without rescan");
    std::cout << "PASS original AVS defaults, typed configuration, padding and explicit unsupported cases\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
