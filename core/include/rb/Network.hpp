#pragma once
#include "Property.hpp"
#include <atomic>
#include <chrono>
#include <memory>

namespace rb {
enum class ServerMode { offline, remote, embedded };
enum class NetworkFailure { disabled, cancelled, timeout, http, protocol, unavailable };
struct NetworkError : Error {
    NetworkFailure kind;
    NetworkError(NetworkFailure k, const std::string &m) : Error(m), kind(k) {};
};
struct ServerConfig {
    ServerMode mode = ServerMode::offline;
    std::string endpoint, profileID = "offline";
};
class EmbeddedServer {
  public:
    virtual ~EmbeddedServer() = default;
    virtual bool available() const = 0;
    virtual void start() = 0;
    virtual void stop() noexcept = 0;
    virtual Bytes handle(const Bytes &) = 0;
};
struct NetworkRequest {
    std::string endpoint;
    Bytes body;
    std::chrono::steady_clock::time_point deadline;
};
struct NetworkResponse {
    int status = 200;
    Bytes body;
};
class NetworkTransport {
  public:
    virtual ~NetworkTransport() = default;
    virtual NetworkResponse send(const NetworkRequest &, const std::atomic_bool &cancelled) = 0;
};
// No production transport is constructed implicitly. Tests inject an in-memory transport.
class NetworkClient {
    ServerConfig config_;
    std::shared_ptr<EmbeddedServer> embedded_;
    std::shared_ptr<NetworkTransport> transport_;
    bool active_ = false;

  public:
    explicit NetworkClient(std::shared_ptr<NetworkTransport> t = {},
                           std::shared_ptr<EmbeddedServer> s = {})
        : embedded_(std::move(s)), transport_(std::move(t)) {}
    NetworkClient(const NetworkClient &) = delete;
    NetworkClient &operator=(const NetworkClient &) = delete;
    ~NetworkClient();
    void configure(ServerConfig);
    const ServerConfig &config() const {
        return config_;
    }
    void beginSession();
    void endSession() {
        active_ = false;
    }
    bool active() const {
        return active_;
    }
    bool embeddedAvailable() const {
        return embedded_ && embedded_->available();
    }
    PropertyDocument exchange(const PropertyDocument &, const std::atomic_bool &,
                              std::chrono::milliseconds timeout = std::chrono::seconds(10));
};
// Route registration restored from 0x10187C30.
struct XrpcModule {
    const char *service;
    const char *module;
};
const std::vector<XrpcModule> &xrpcModules();
} // namespace rb
