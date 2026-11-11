#include "rb/AvsMount.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
using namespace rb;
namespace {
std::string hex(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : value) { out += digits[c >> 4]; out += digits[c & 15]; }
    return out.empty() ? "-" : out;
}
class RecordingBackend final : public AvsFilesystemBackend {
public:
    int failedCall = -1, calls = 0;
    int32_t response = 0;
    std::vector<std::string> trace;
    int32_t result(int32_t normal = 1) { return ++calls == failedCall ? response : normal; }
    uintptr_t createFilesystem(const AvsFilesystemType &type) override {
        const auto index = size_t(&type - avsFilesystemTypes().data());
        trace.push_back("F:" + std::string(type.name) + ":" + std::to_string(type.originalFactoryAddress));
        return 0x50000000u + index * 16;
    }
    int32_t addFilesystem(uintptr_t descriptor) override {
        trace.push_back("A:" + std::to_string(descriptor)); return result();
    }
    int32_t mount(const AvsMountRequest &r) override {
        trace.push_back("M:" + hex(r.path) + ":" + hex(r.device) + ":" + hex(r.filesystem) + ":" + hex(r.options));
        return result();
    }
    int32_t openMountTable(std::string_view path) override { trace.push_back("O:" + hex(path)); return result(17); }
    int32_t applyMountTable(int32_t handle, std::string_view selection) override {
        trace.push_back("T:" + std::to_string(handle) + ":" + hex(selection)); return result();
    }
};
void require(bool value) { if (!value) throw std::runtime_error("AVS mount assertion failed"); }
void writeField(AvsBootConfigImage &config, size_t offset, std::string_view value) {
    std::copy(value.begin(), value.end(), config.begin() + offset);
    config[offset + value.size()] = 0;
}
AvsBootConfigImage completeConfig() {
    AvsBootConfigImage config{};
    writeField(config, 138, "fs"); writeField(config, 155, "."); writeField(config, 412, "posix=1");
    writeField(config, 669, "nvram"); writeField(config, 686, "memory"); writeField(config, 943, "sz_block=512");
    writeField(config, 1200, "fs"); writeField(config, 1217, "raw"); writeField(config, 1474, "posix=1");
    writeField(config, 1731, "/config/mounttable.xml");
    return config;
}
std::vector<std::string> expectedTrace() {
    // Independent expected factory order from the original 0x10068ABC table.
    const char *names[] = {"default", "nvram", "imagefs", "mirrorfs", "cryptfs", "nvram2"};
    const uint32_t factories[] = {0x10029510, 0x1001DAE0, 0x10023270, 0x1002B270, 0x1003B550, 0x1003CA30};
    std::vector<std::string> trace;
    for (size_t i = 0; i < 6; ++i) {
        trace.push_back("F:" + std::string(names[i]) + ":" + std::to_string(factories[i]));
        trace.push_back("A:" + std::to_string(0x50000000u + i*16));
    }
    trace.push_back("M:" + hex("/") + ':' + hex(".") + ':' + hex("fs") + ':' + hex("posix=1"));
    trace.push_back("M:" + hex("/dev/nvram") + ':' + hex("memory") + ':' + hex("nvram") + ':' + hex("sz_block=512"));
    trace.push_back("M:" + hex("/dev/raw") + ':' + hex("raw") + ':' + hex("fs") + ':' + hex("posix=1"));
    trace.push_back("O:" + hex("/config/mounttable.xml"));
    trace.push_back("T:17:" + hex("ALL"));
    return trace;
}
void testBootStage() {
    auto config = completeConfig();
    const auto before = config;
    const auto fullTrace = expectedTrace();
    RecordingBackend success;
    require(runAvsFilesystemBoot(config, success) == 17);
    require(success.calls == 11 && success.trace == fullTrace && config == before);

    // Each nonpositive register/mount/open response stops before the next call.
    for (int fail = 1; fail <= 10; ++fail) {
        for (int32_t response : {INT32_MIN, -1, 0}) {
            RecordingBackend backend; backend.failedCall = fail; backend.response = response;
            const auto stage = fail <= 6 ? AvsFilesystemFailure::registration :
                               fail <= 9 ? AvsFilesystemFailure::mount : AvsFilesystemFailure::mountTable;
            const char *subjects[] = {"default", "nvram", "imagefs", "mirrorfs", "cryptfs", "nvram2",
                                      "/", "/dev/nvram", "/dev/raw", "/config/mounttable.xml"};
            try { runAvsFilesystemBoot(config, backend); require(false); }
            catch (const AvsFilesystemBootError &e) {
                require(e.stage == stage && e.code == response && e.subject == subjects[fail-1]);
            }
            const size_t count = fail <= 6 ? size_t(fail*2) : size_t(fail+6);
            require(backend.calls == fail && backend.trace == std::vector<std::string>(fullTrace.begin(), fullTrace.begin()+count));
        }
    }
    for (int32_t response : {INT32_MIN, -1, 0, INT32_MAX}) {
        RecordingBackend backend; backend.failedCall = 11; backend.response = response;
        require(runAvsFilesystemBoot(config, backend) == 17);
        require(backend.trace == fullTrace && backend.calls == 11);
    }
    // Positive open handles are forwarded unchanged and returned to the caller.
    RecordingBackend largeHandle; largeHandle.failedCall = 10; largeHandle.response = INT32_MAX;
    require(runAvsFilesystemBoot(config, largeHandle) == INT32_MAX);
    require(largeHandle.trace.back() == "T:2147483647:" + hex("ALL"));

    config[155] = 0;
    RecordingBackend emptyRoot;
    try { runAvsFilesystemBoot(config, emptyRoot); require(false); }
    catch (const AvsFilesystemBootError &e) {
        require(e.stage == AvsFilesystemFailure::emptyRootDevice && e.subject == "/config/fs/root/device");
    }
    require(emptyRoot.trace.empty());

    // All optional-device combinations, including unreadable unused fields.
    for (unsigned mask = 0; mask < 4; ++mask) {
        config = completeConfig(); config[1731] = 0;
        for (size_t i = 0; i < 2; ++i) if (!(mask & (1u << i))) {
            const size_t type = 669+i*531;
            config[type+17] = 0;
            std::fill_n(config.begin()+type, 17, 'x');
            std::fill_n(config.begin()+type+274, 257, 'x');
        }
        RecordingBackend backend;
        require(runAvsFilesystemBoot(config, backend) == 0);
        auto expected = std::vector<std::string>(fullTrace.begin(), fullTrace.begin()+13);
        if (mask & 1) expected.push_back(fullTrace[13]);
        if (mask & 2) expected.push_back(fullTrace[14]);
        require(backend.trace == expected);
    }

    config = completeConfig(); writeField(config, 1731, std::string(255, 'x'));
    RecordingBackend longestPath;
    require(runAvsFilesystemBoot(config, longestPath) == 17);
    require(longestPath.trace[15] == "O:" + hex(std::string(255, 'x')));
    config[1731+255] = 'x'; config[1731+256] = 0;
    RecordingBackend invalidPath;
    try { runAvsFilesystemBoot(config, invalidPath); require(false); }
    catch (const AvsFilesystemBootError &e) {
        require(e.stage == AvsFilesystemFailure::unterminatedField && e.subject == "1731");
    }
    require(invalidPath.trace == std::vector<std::string>(fullTrace.begin(), fullTrace.begin()+15));
}
}
int main(int argc, char **argv) { try {
    if (argc > 1 && std::string(argv[1]) == "--batch") {
        int failedCall; int32_t response; std::string bytes;
        while (std::cin >> failedCall >> response >> bytes) {
            AvsBootConfigImage config{}; require(bytes.size() == config.size() * 2);
            for (size_t i = 0; i < config.size(); ++i) config[i] = uint8_t(std::stoul(bytes.substr(i*2, 2), nullptr, 16));
            RecordingBackend backend; backend.failedCall = failedCall; backend.response = response;
            try { const auto value = runAvsFilesystemBoot(config, backend); std::cout << "R:" << value; }
            catch (const AvsFilesystemBootError &e) { std::cout << "E:" << int(e.stage) << ':' << e.code; }
            for (const auto &event : backend.trace) std::cout << ' ' << event;
            std::cout << '\n';
        }
        return 0;
    }
    RecordingBackend backend; backend.failedCall = 1;
    require(registerAvsFilesystem(nullptr, 0, backend)); require(backend.calls == 0);
    require(!registerAvsFilesystem("default", 0, backend)); require(backend.calls == 1);
    AvsMountRequest request{"/", "device", "default", "ro"};
    require(mountAvsFilesystem(nullptr, request, backend)); require(backend.calls == 1);
    request.device.clear(); require(mountAvsFilesystem("/", request, backend)); require(backend.calls == 1);
    request.device = "device";
    for (int32_t result : {INT32_MIN, -1, 0, 1, INT32_MAX}) {
        backend.failedCall = backend.calls + 1; backend.response = result;
        require(mountAvsFilesystem("/dev/raw", request, backend) == (result > 0));
        backend.failedCall = backend.calls + 1;
        require(registerAvsFilesystem("default", 0, backend) == (result > 0));
    }
    AvsBootConfigImage config{}; config[155] = 'x'; config[156] = 0;
    // Bounded native reads reject malformed fields; original C did not bound them.
    std::fill(config.begin() + 138, config.begin() + 155, 'f');
    try { runAvsFilesystemBoot(config, backend); require(false); }
    catch (const AvsFilesystemBootError &e) { require(e.stage == AvsFilesystemFailure::unterminatedField); }
    testBootStage();
    std::cout << "PASS AVS filesystem ordered boot, stage failures, skips, mount table and bounded fields\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; } }
