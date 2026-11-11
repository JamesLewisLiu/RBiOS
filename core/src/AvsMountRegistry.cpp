#include "rb/AvsMountRegistry.hpp"
#include "rb/AvsDescriptors.hpp"
#include "rb/AvsPath.hpp"
#include "rb/AvsDefaultMountOptions.hpp"
#include <algorithm>
#include <array>
#include <list>
#include <mutex>
#include <unordered_map>
namespace rb {
namespace {
constexpr int32_t error(uint32_t code) { return int32_t(0x80070000u | code); }
}
struct AvsMountRegistry::State {
    struct Mount {
        State *owner = nullptr;
        AvsResolvedPath target;
        std::string device;
        uint32_t token = 0;
        uint32_t references = 0;
        bool posix = false, pending = true;
    };
    explicit State(uint32_t descriptors, uint32_t mounts) : descriptors(descriptors), capacity(mounts) {}
    ~State() {
        // Open files own this shared state, so no live file references remain.
        while (!entries.empty()) descriptors.release(entries.begin()->first);
    }
    static int32_t destroy(uintptr_t payload) {
        auto *mount = reinterpret_cast<Mount *>(payload);
        auto &owner = *mount->owner;
        std::lock_guard<std::recursive_mutex> lock(owner.mutex);
        owner.buckets[bucket(mount->target)].remove(mount);
        owner.entries.erase(mount->token);
        return 0;
    }
    static size_t depth(const AvsResolvedPath &path) { return uint32_t(path.status) >> 16; }
    static size_t bucket(const AvsResolvedPath &path) { return depth(path)*8+(path.prefixHashes.back()&7); }
    Mount *exact(const AvsResolvedPath &path) const {
        for (auto *entry : buckets[bucket(path)])
            if (!entry->pending && entry->target.path == path.path &&
                entry->target.prefixHashes.back() == path.prefixHashes.back()) return entry;
        return nullptr;
    }
    Mount *resolve(const AvsResolvedPath &path) const {
        // 0x1002C820 visits depth buckets from the input depth down to root.
        for (int d = int(depth(path)); d >= 0; --d) {
            for (auto *entry : buckets[size_t(d)*8+(path.prefixHashes[size_t(d)]&7)]) {
                if (entry->pending) continue;
                if (d == 0) return entry;
                const auto &prefix = entry->target.path;
                if (path.prefixHashes[size_t(d)] != entry->target.prefixHashes.back()) continue;
                if (path.path.compare(0, prefix.size(), prefix) == 0 &&
                    (path.path.size() == prefix.size() || path.path[prefix.size()] == '/')) return entry;
            }
        }
        return nullptr;
    }
    static int32_t hostPath(const Mount &mount, const AvsResolvedPath &path, std::string &out) {
        // 0x1002BD00: exact mount -> '/', root -> entire path, otherwise suffix.
        const std::string suffix = path.path.size() == mount.target.path.size() ? "/" :
            depth(mount.target) == 0 ? path.path : path.path.substr(mount.target.path.size());
        const auto value = mount.device + suffix; // 0x1002A8B0 concatenates, no path join normalization.
        if (value.size() >= 256) return error(36);
        out = value;
#ifdef _WIN32
        std::replace(out.begin(), out.end(), '/', '\\'); // Original 0x1002A900.
#endif
        return 0;
    }
    mutable std::recursive_mutex mutex;
    AvsDescriptors descriptors;
    uint32_t capacity;
    bool defaultRegistered = false;
    std::array<std::list<Mount *>, 65*8> buckets;
    std::unordered_map<uint32_t, std::unique_ptr<Mount>> entries;
};
AvsMountRegistry::AvsMountRegistry(uint32_t descriptors, uint32_t mounts)
    : state_(std::make_shared<State>(descriptors, mounts)) {}
void AvsMountRegistry::registerReadOnlyDefaultFilesystem() {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    state_->defaultRegistered = true;
}
int32_t AvsMountRegistry::mount(const AvsMountRequest &request) {
    auto target = resolveAvsPath("", request.path, 128);
    if (request.filesystem.empty()) return error(22);
    if (target.status < 0) return target.status;
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    if (!state_->defaultRegistered || request.filesystem != "fs") return error(19);
    // This native capability is intentionally separate from the full AVS
    // registration API. No nvram/imagefs/cryptfs backend is claimed.
    const auto token = state_->descriptors.allocate(10, nullptr, 0);
    if (!token) return 0; // Do not bind the original reserved token zero on exhaustion.
    auto fail = [&](int32_t value) { state_->descriptors.release(token); return value; };
    if (state_->exact(target)) return fail(error(17));
    if (state_->entries.size() >= state_->capacity) return fail(error(12));
    AvsDefaultMountOptions options;
    const auto optionResult = parseAvsDefaultMountOptions(request.options, options);
    if (optionResult < 0) return fail(optionResult);
    if (request.device.size() >= 256 || request.device.find('\0') != std::string::npos) return fail(error(36));
    try {
        auto entry = std::make_unique<State::Mount>();
        entry->owner = state_.get(); entry->target = std::move(target);
        entry->token = token; entry->device = request.device; entry->posix = options.posix;
        auto *raw = entry.get();
        state_->entries.emplace(token, std::move(entry));
        state_->buckets[State::bucket(raw->target)].push_front(raw);
        raw->pending = false;
        state_->descriptors.replace(token, State::destroy, reinterpret_cast<uintptr_t>(raw));
    } catch (const std::bad_alloc &) {
        const auto found = state_->entries.find(token);
        if (found != state_->entries.end()) {
            state_->buckets[State::bucket(found->second->target)].remove(found->second.get());
            state_->entries.erase(found);
        }
        return fail(error(12));
    }
    // The default mount callback accepts a device without opening it. Missing
    // device paths therefore fail at actual file open, not mount registration.
    return int32_t(token);
}
int32_t AvsMountRegistry::unmount(std::string_view path) {
    const auto target = resolveAvsPath("", path, 128);
    if (target.status < 0) return target.status;
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    auto *mount = state_->exact(target);
    if (!mount) return error(19);
    if (mount->references) return error(16);
    mount->pending = true;
    return state_->descriptors.release(mount->token);
}
int32_t AvsMountRegistry::convertPath(std::string_view path, std::string &host) const {
    const auto target = resolveAvsPath("/", path);
    if (target.status < 0) return target.status;
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    const auto *mount = state_->resolve(target);
    return mount ? State::hostPath(*mount, target, host) : error(19);
}
int32_t AvsMountRegistry::open(std::string_view path, std::unique_ptr<AvsMountedFile> &output,
                              uint32_t flags, uint16_t permissions) {
    if (output) return error(16);
    const auto target = resolveAvsPath("/", path);
    if (target.status < 0) return target.status;
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    auto *mount = state_->resolve(target);
    if (!mount) return error(19);
    std::string host;
    const auto result = State::hostPath(*mount, target, host);
    if (result < 0) return result;
    auto file = std::unique_ptr<AvsMountedFile>(new AvsMountedFile(state_, mount->token));
    ++mount->references;
    const auto status = file->file_.open(host, flags, permissions, mount->posix);
    if (status < 0) return status; // File destructor releases the acquired mount reference.
    output = std::move(file);
    return 0;
}
size_t AvsMountRegistry::mountedCount() const {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    return state_->entries.size();
}
AvsMountedFile::AvsMountedFile(std::shared_ptr<AvsMountRegistry::State> state, uint32_t mount)
    : state_(std::move(state)), mount_(mount) {}
AvsMountedFile::~AvsMountedFile() { close(); }
int32_t AvsMountedFile::close() {
    if (!state_) return error(9);
    const auto status = file_.isOpen() ? file_.close() : 0;
    {
        std::lock_guard<std::recursive_mutex> lock(state_->mutex);
        const auto found = state_->entries.find(mount_);
        if (found != state_->entries.end()) --found->second->references;
    }
    state_.reset();
    return status;
}
}
