import XCTest
import UIKit
@testable import RBiOS

final class StageFrameInterpolationTests: XCTestCase {
    private func draw(_ x: CGFloat, id: String = "1:head", name: String = "note") -> [String: Any] {
        ["id": id, "name": name, "layer": 40,
         "quad": [CGPoint(x: x, y: 20), CGPoint(x: x + 10, y: 20),
                  CGPoint(x: x, y: 10), CGPoint(x: x + 10, y: 10)].map { NSValue(cgPoint: $0) }]
    }
    private func x(_ draws: [[String: Any]]?) throws -> CGFloat {
        let rows = try XCTUnwrap(draws)
        return try XCTUnwrap(rows.first?["quad"] as? [NSValue]).first!.cgPointValue.x
    }
    func testTwo120HzSamplesInterpolateConsecutive60HzStates() throws {
        var frames = StageFrameInterpolation()
        frames.submit([draw(0)], tick: 0, at: 0)
        XCTAssertEqual(try x(frames.frame(at: 0)), 0)
        frames.submit([draw(12)], tick: 1, at: 1)
        XCTAssertEqual(try x(frames.frame(at: 1)), 0)
        XCTAssertEqual(try x(frames.frame(at: 1 + 1.0 / 120)), 6, accuracy: 0.001)
        XCTAssertEqual(try x(frames.frame(at: 1 + 1.0 / 60)), 12, accuracy: 0.001)
        // A stall holds the latest complete state rather than extrapolating movement.
        let stalled = frames.frame(at: 10)
        if let stalled = stalled { XCTAssertEqual(try x(stalled), 12) }
        XCTAssertNil(frames.frame(at: 11))
    }
    func testSkippedTicksAndResetSnapWithoutOldMovement() throws {
        var frames = StageFrameInterpolation()
        frames.submit([draw(0)], tick: 0, at: 0)
        frames.submit([draw(24)], tick: 2, at: 1)
        XCTAssertEqual(try x(frames.frame(at: 1)), 24)
        frames.reset(); XCTAssertNil(frames.frame(at: 2))
        frames.submit([draw(90)], tick: 3, at: 2)
        XCTAssertEqual(try x(frames.frame(at: 2)), 90)
    }
    func testDuplicateAndOutOfOrderSubmissionsDoNotRestartMovement() throws {
        var frames = StageFrameInterpolation()
        frames.submit([draw(0)], tick: 1, at: 0)
        frames.submit([draw(12)], tick: 2, at: 1)
        frames.submit([draw(100)], tick: 2, at: 1.005)
        frames.submit([draw(200)], tick: 0, at: 1.006)
        XCTAssertEqual(try x(frames.frame(at: 1 + 1.0 / 120)), 6, accuracy: 0.001)
    }
    func testNewRemovedAndChangedTexturesAreNeverBlended() throws {
        var frames = StageFrameInterpolation()
        frames.submit([draw(0), draw(10, id: "2:head")], tick: 0, at: 0)
        frames.submit([draw(12, name: "changed"), draw(30, id: "3:head")], tick: 1, at: 1)
        let result = try XCTUnwrap(frames.frame(at: 1))
        XCTAssertEqual(result.count, 2)
        XCTAssertEqual(result.compactMap { $0["id"] as? String }, ["1:head", "3:head"])
        XCTAssertEqual(try x(result), 12)
    }
    func testTeleportAndHUDChangesSnapImmediately() throws {
        var frames = StageFrameInterpolation()
        frames.submit([draw(0), draw(0, id: "score:0")], tick: 0, at: 0)
        frames.submit([draw(200), draw(12, id: "score:0")], tick: 1, at: 1)
        let result = try XCTUnwrap(frames.frame(at: 1))
        XCTAssertEqual(try x(result), 200)
        XCTAssertEqual(try x([result[1]]), 12)
    }
    func testFrameRateRequestRespectsTheAttachedScreen() throws {
        let surface = try XCTUnwrap(GameSurface())
        XCTAssertEqual(surface.requestedFramesPerSecond, min(120, UIScreen.main.maximumFramesPerSecond))
    }
}
