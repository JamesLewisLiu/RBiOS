import XCTest
import MetalKit
@testable import RBiOS

final class OfflineURLProtocol: URLProtocol {
    static var status = 200
    static var requests = 0
    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        Self.requests += 1
        let response = HTTPURLResponse(url: request.url!, statusCode: Self.status, httpVersion: "HTTP/1.1", headerFields: nil)!
        client?.urlProtocol(self, didReceive: response, cacheStoragePolicy: .notAllowed)
        client?.urlProtocol(self, didLoad: Data([0xa0, 0x42]))
        client?.urlProtocolDidFinishLoading(self)
    }
    override func stopLoading() {}
}
final class AdapterTests: XCTestCase {
    func testGameSurfaceKeepsFixedDrawableSize() throws {
        guard let surface = GameSurface() else { throw XCTSkip("Metal is unavailable on this simulator") }
        let previousWindow = UIApplication.shared.windows.first { $0.isKeyWindow }
        let window = UIWindow(frame: UIScreen.main.bounds)
        let host = UIViewController(); window.rootViewController = host; window.makeKeyAndVisible()
        window.layoutIfNeeded(); host.view.addSubview(surface)
        defer { surface.removeFromSuperview(); window.isHidden = true; previousWindow?.makeKey() }
        surface.frame = CGRect(x: 0, y: 0, width: 1024, height: 1366)
        for scale in [0.5, 0.75, 1.0] {
            surface.fraction = scale; surface.layoutIfNeeded()
            XCTAssertEqual(surface.configuredDrawableSize, CGSize(width: 768, height: 1360))
            XCTAssertEqual(surface.configuredSceneSize, CGSize(width: 768, height: 1360))
            let expected = RBBridge.displayRect(in: surface.safeAreaLayoutGuide.layoutFrame, fraction: scale)
            XCTAssertEqual(surface.displayRect, expected)
            XCTAssertGreaterThan(surface.displayRect.width, 0)
            XCTAssertGreaterThan(surface.displayRect.height, 0)
        }
        surface.frame = CGRect(x: 0, y: 0, width: 1366, height: 1024)
        surface.setNeedsLayout(); surface.layoutIfNeeded()
        XCTAssertEqual(surface.configuredDrawableSize, CGSize(width: 768, height: 1360))
    }
    func testScalingRoundtrip() {
        for scale in [0.5, 0.75, 1.0] {
            let rect = RBBridge.displayRect(in: CGRect(x: 0, y: 24, width: 1024, height: 1320), fraction: scale)
            let p = RBBridge.gamePoint(CGPoint(x: rect.midX, y: rect.midY), display: rect)!.cgPointValue
            XCTAssertEqual(p.x, 384, accuracy: 0.0001); XCTAssertEqual(p.y, 680, accuracy: 0.0001)
            XCTAssertNil(RBBridge.gamePoint(CGPoint(x: rect.minX - 1, y: rect.midY), display: rect))
        }
    }
    func testDefaultTransportDoesNotSend() {
        let config = URLSessionConfiguration.ephemeral; config.protocolClasses = [OfflineURLProtocol.self]
        let session = URLSession(configuration: config); defer { session.invalidateAndCancel() }
        OfflineURLProtocol.requests = 0
        let transport = RemoteTransport(session: session)
        let task = transport.send(to: URL(string: "https://example.invalid")!, body: Data()) { result in
            if case .success = result { XCTFail("network must default to disabled") }
        }
        XCTAssertNil(task); XCTAssertEqual(OfflineURLProtocol.requests, 0)
    }
    func testHTTPAdapterThroughInMemoryProtocol() {
        let config = URLSessionConfiguration.ephemeral; config.protocolClasses = [OfflineURLProtocol.self]
        let session = URLSession(configuration: config); defer { session.invalidateAndCancel() }
        let transport = RemoteTransport(session: session, allowLiveNetwork: true)
        let done = expectation(description: "mock reply")
        transport.send(to: URL(string: "https://example.invalid")!, body: Data([1, 2, 3])) { result in
            switch result { case .success(let data): XCTAssertEqual(data, Data([0xa0, 0x42])); case .failure(let error): XCTFail(String(describing: error)) }; done.fulfill()
        }
        wait(for: [done], timeout: 2)
    }
}
