#include "rb/Network.hpp"
#include <cctype>

namespace rb {
const std::vector<XrpcModule> &xrpcModules() {
    static const std::vector<XrpcModule> modules = {
        {"services", "services"}, {"cardmng", "cardmng"}, {"local2", "info"}, {"local2", "player"},
        {"lobby2", "lobby"},      {"local2", "shop"},     {"local2", "pcb"}};
    return modules;
}
NetworkClient::~NetworkClient() {
    if (config_.mode == ServerMode::embedded && embedded_)
        embedded_->stop();
}
void NetworkClient::configure(ServerConfig c) {
    if (active_)
        throw NetworkError(NetworkFailure::unavailable,
                           "end active session before changing server");
    if (c.mode != ServerMode::offline && c.mode != ServerMode::remote &&
        c.mode != ServerMode::embedded)
        throw NetworkError(NetworkFailure::protocol, "invalid server mode");
    if (c.profileID.empty() ||
        c.profileID.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") !=
            std::string::npos)
        throw NetworkError(NetworkFailure::protocol, "invalid backend profile ID");
    if (c.mode == ServerMode::remote) {
        if (c.endpoint.rfind("https://", 0) != 0 && c.endpoint.rfind("http://", 0) != 0)
            throw NetworkError(NetworkFailure::protocol, "server must be HTTP(S)");
        auto authority = c.endpoint.substr(c.endpoint.find("://") + 3);
        authority = authority.substr(0, authority.find('/'));
        if (authority.empty() || authority.find('@') != std::string::npos ||
            c.endpoint.find_first_of("\r\n\t ") != std::string::npos)
            throw NetworkError(NetworkFailure::protocol, "invalid server endpoint");
    }
    if (c.mode == ServerMode::embedded && !embeddedAvailable())
        throw NetworkError(NetworkFailure::unavailable, "embedded server source not integrated");
    if (c.mode == ServerMode::embedded && config_.mode != ServerMode::embedded)
        embedded_->start();
    if (config_.mode == ServerMode::embedded && c.mode != ServerMode::embedded)
        embedded_->stop();
    config_ = std::move(c);
}
void NetworkClient::beginSession() {
    if (active_)
        throw NetworkError(NetworkFailure::unavailable, "session already active");
    active_ = true;
}
PropertyDocument NetworkClient::exchange(const PropertyDocument &doc,
                                         const std::atomic_bool &cancelled,
                                         std::chrono::milliseconds timeout) {
    if (config_.mode == ServerMode::offline)
        throw NetworkError(NetworkFailure::disabled, "network disabled in offline mode");
    if (cancelled.load())
        throw NetworkError(NetworkFailure::cancelled, "request cancelled");
    if (timeout.count() <= 0)
        throw NetworkError(NetworkFailure::timeout, "request timed out");
    NetworkRequest request{config_.endpoint, doc.encode(),
                           std::chrono::steady_clock::now() + timeout};
    NetworkResponse response;
    if (config_.mode == ServerMode::embedded)
        response = {200, embedded_->handle(request.body)};
    else {
        if (!transport_)
            throw NetworkError(NetworkFailure::disabled, "no network transport installed");
        response = transport_->send(request, cancelled);
    }
    if (cancelled.load())
        throw NetworkError(NetworkFailure::cancelled, "request cancelled");
    if (std::chrono::steady_clock::now() > request.deadline)
        throw NetworkError(NetworkFailure::timeout, "request timed out");
    if (response.status < 200 || response.status >= 300)
        throw NetworkError(NetworkFailure::http, "HTTP status " + std::to_string(response.status));
    try {
        return PropertyDocument::decode(response.body);
    } catch (const Error &e) {
        throw NetworkError(NetworkFailure::protocol, e.what());
    }
}
} // namespace rb
