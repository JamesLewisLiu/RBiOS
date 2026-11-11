#include "rb/AvsBootConfig.hpp"
#include "rb/AvsConfigImport.hpp"
#include "rb/AvsMount.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace rb;
namespace {
// This records the data contract between recovered phases. It does not run
// the intervening AVS descriptor/thread/heap services or claim a full boot.
struct MountRecorder final : AvsFilesystemBackend {
    size_t factories = 0, registrations = 0;
    std::vector<AvsMountRequest> mounts;
    uintptr_t createFilesystem(const AvsFilesystemType &) override { return ++factories; }
    int32_t addFilesystem(uintptr_t descriptor) override {
        if (descriptor != ++registrations) throw std::runtime_error("descriptor order");
        return 1;
    }
    int32_t mount(const AvsMountRequest &request) override { mounts.push_back(request); return 1; }
    int32_t openMountTable(std::string_view) override { throw std::runtime_error("unexpected table open"); }
    int32_t applyMountTable(int32_t, std::string_view) override { throw std::runtime_error("unexpected table apply"); }
};
Property scalar(const char *name, uint8_t type, Bytes value) {
    Property p; p.name = name; p.type = type; p.value = std::move(value); return p;
}
Property group(const char *name, std::vector<Property> children) {
    Property p; p.name = name; p.children = std::move(children); return p;
}
void checkPhaseDataContract() {
    for (uint8_t product : {0, 1}) {
        auto tree = group("config", {
            group("mode", {scalar("product", 52, {product})}),
            group("thread", {scalar("mutex_delay", 5, {0, 5})}),
            group("time", {scalar("debug_local_offset", 6, {255, 255, 255, 254})}),
            group("fs", {
                group("root", {scalar("option", 11, {'p','o','s','i','x','=','0',0})}),
                group("nvram", {scalar("device", 11, {'n','v',0}), scalar("fstype", 11, {'f','s',0})}),
                group("raw", {scalar("device", 11, {'r','a','w',0})})
            })
        });
        auto image = importAvsBootConfig(&tree);
        const auto imported = image;
        const auto corrections = applyAvsProductMode(image, image[0] != 0);
        if (corrections.size() != (product ? 3u : 0u)) throw std::runtime_error("imported product mode");
        if (!product && image != imported) throw std::runtime_error("non-product imported image changed");
        if (product && (corrections[0].previous != 1 || corrections[1].previous != 5 || corrections[2].previous != -2))
            throw std::runtime_error("imported correction values");
        MountRecorder recorder;
        const auto handle = runAvsFilesystemBoot(image, recorder);
        if (handle != 0 || recorder.factories != 6 || recorder.registrations != 6 || recorder.mounts.size() != 3)
            throw std::runtime_error("imported mount dispatch");
        const char *paths[] = {"/", "/dev/nvram", "/dev/raw"};
        const char *devices[] = {".", "nv", "raw"};
        const char *options[] = {"posix=0", "sz_block=512,nr_block=32,nr_mirror=4", "posix=1"};
        for (size_t i = 0; i < 3; ++i) {
            const auto &request = recorder.mounts[i];
            if (request.path != paths[i] || request.device != devices[i] || request.filesystem != "fs" || request.options != options[i])
                throw std::runtime_error("imported mount argument mismatch");
        }
    }
}
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        unsigned enabled; std::string hex;
        while (std::cin >> enabled >> hex) {
            AvsBootConfigImage config{};
            if (hex.size() != config.size() * 2) throw std::runtime_error("config image size");
            for (size_t i = 0; i < config.size(); ++i) config[i] = uint8_t(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
            const auto events = applyAvsProductMode(config, enabled != 0);
            unsigned mask = 0; for (const auto &e : events) mask |= 1u << unsigned(e.field);
            std::cout << mask << ' ';
            constexpr char digits[] = "0123456789abcdef";
            for (auto byte : config) std::cout << digits[byte >> 4] << digits[byte & 15];
            for (const auto &e : events) std::cout << ' ' << unsigned(e.field) << ':' << e.previous;
            std::cout << '\n';
        }
        return 0;
    }
    for (unsigned flags = 0; flags < 8; ++flags) {
        AvsBootConfigImage before; before.fill(0xA5);
        before[1] = flags & 1 ? 255 : 0;
        before[118] = before[119] = flags & 2 ? 255 : 0;
        for (unsigned i = 0; i < 4; ++i) before[2268+i] = flags & 4 ? 255 : 0;
        auto untouched = before;
        if (!applyAvsProductMode(untouched, false).empty() || untouched != before) throw std::runtime_error("non-product mutation");
        auto after = before; const auto events = applyAvsProductMode(after, true);
        unsigned mask = 0;
        for (const auto &e : events) {
            mask |= 1u << unsigned(e.field);
            const int32_t expected[] = {255, 65535, -1};
            if (e.previous != expected[unsigned(e.field)]) throw std::runtime_error("wrong log value");
        }
        if (mask != flags) throw std::runtime_error("missing original log event");
        for (size_t i = 0; i < after.size(); ++i) {
            const bool changed = i == 1 || i == 118 || i == 119 || (i >= 2268 && i < 2272);
            if (after[i] != (changed ? 0 : before[i])) throw std::runtime_error("unrelated configuration changed");
        }
        if (!applyAvsProductMode(after, true).empty()) throw std::runtime_error("duplicate correction logs");
    }
    checkPhaseDataContract();
    std::cout << "PASS AVS product-mode fields and import/correction/mount data contract\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
