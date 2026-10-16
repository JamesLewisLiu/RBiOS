import Foundation

enum RemoteTransportError: Error { case liveNetworkDisabled, invalidURL, httpStatus(Int), invalidResponse, responseTooLarge }

// Dormant HTTP transport. App creates none; the wire-envelope/EA3 integration remains pending.
final class RemoteTransport {
    private let session: URLSession
    private let liveNetworkAllowed: Bool
    init(session: URLSession, allowLiveNetwork: Bool = false) { self.session = session; self.liveNetworkAllowed = allowLiveNetwork }
    @discardableResult
    func send(to url: URL, body: Data, timeout: TimeInterval = 10, completion: @escaping (Result<Data, Error>) -> Void) -> URLSessionDataTask? {
        guard liveNetworkAllowed else { completion(.failure(RemoteTransportError.liveNetworkDisabled)); return nil }
        guard ["http", "https"].contains(url.scheme?.lowercased() ?? ""), url.host != nil, url.user == nil, url.password == nil else { completion(.failure(RemoteTransportError.invalidURL)); return nil }
        var request = URLRequest(url: url); request.httpMethod = "POST"; request.httpBody = body; request.timeoutInterval = max(0.1, timeout)
        request.setValue("application/octet-stream", forHTTPHeaderField: "Content-Type")
        let task = session.dataTask(with: request) { data, response, error in
            if let error = error { completion(.failure(error)); return }
            guard let response = response as? HTTPURLResponse else { completion(.failure(RemoteTransportError.invalidResponse)); return }
            guard (200..<300).contains(response.statusCode) else { completion(.failure(RemoteTransportError.httpStatus(response.statusCode))); return }
            guard let data = data, data.count <= 16 * 1024 * 1024 else { completion(.failure(RemoteTransportError.responseTooLarge)); return }
            completion(.success(data))
        }
        task.resume(); return task
    }
}
