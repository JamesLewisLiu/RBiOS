import XCTest
@testable import RBiOS

final class StageSoundPlayerTests: XCTestCase {
    func testOriginalPCMRetainsStereoOrderSignedExtremesAndFrameCount() throws {
        let pcm = StageSoundPCM(sampleRate: 44100, bytes: Data([0, 128, 255, 127, 0, 0, 0, 64]))
        let buffer = try pcm.makeBuffer()
        XCTAssertEqual(buffer.frameLength, 2)
        XCTAssertEqual(buffer.format.sampleRate, 44100)
        let channels = try XCTUnwrap(buffer.floatChannelData)
        XCTAssertEqual(channels[0][0], -1)
        XCTAssertEqual(channels[1][0], 32767.0 / 32768, accuracy: 0.00001)
        XCTAssertEqual(channels[0][1], 0)
        XCTAssertEqual(channels[1][1], 0.5)
    }
    func testInvalidPCMIsRejectedBeforeScheduling() {
        for pcm in [StageSoundPCM(sampleRate: .nan, bytes: Data([0, 0, 0, 0])),
                    StageSoundPCM(sampleRate: 44100, bytes: Data([0, 0, 0])),
                    StageSoundPCM(sampleRate: 44100, bytes: Data())] {
            XCTAssertThrowsError(try pcm.makeBuffer())
        }
    }
    func testPolyphonyStealsOldestAndIgnoresItsLateCompletion() {
        var pool = StageSoundVoicePool(count: 2)
        let first = pool.acquire(), second = pool.acquire(), replacement = pool.acquire()
        XCTAssertNotEqual(first.0, second.0)
        XCTAssertEqual(replacement.0, first.0)
        pool.release(first.0, lease: first.1)
        XCTAssertEqual(pool.leases[replacement.0], replacement.1)
        pool.release(second.0, lease: second.1)
        XCTAssertEqual(pool.acquire().0, second.0)
        pool.cancel()
        pool.release(replacement.0, lease: replacement.1)
        XCTAssertEqual(pool.leases, [0, 0])
    }
    func testPreparationFailureReturnsToMainWithoutPlayback() {
        let player = StageSoundPlayer(decode: { _ in throw StageSoundError.invalidPCM })
        let completed = expectation(description: "original decoder failure")
        player.prepare(["hit": URL(fileURLWithPath: "/unused")]) { result in
            XCTAssertTrue(Thread.isMainThread)
            if case .success = result { XCTFail("Preparation should propagate decoder errors") }
            completed.fulfill()
        }
        wait(for: [completed], timeout: 3)
        player.stop()
    }
    func testStopInvalidatesInFlightPreparation() {
        let decoding = DispatchSemaphore(value: 0), release = DispatchSemaphore(value: 0)
        let player = StageSoundPlayer(decode: { _ in
            decoding.signal(); _ = release.wait(timeout: .now() + 2)
            return StageSoundPCM(sampleRate: 44100, bytes: Data([0, 0, 0, 0]))
        })
        let completed = expectation(description: "cancelled preparation")
        player.prepare(["hit": URL(fileURLWithPath: "/unused")]) { result in
            if case .success = result { XCTFail("Stopped preparation must not become ready") }
            completed.fulfill()
        }
        XCTAssertEqual(decoding.wait(timeout: .now() + 2), .success)
        player.stop(); release.signal()
        wait(for: [completed], timeout: 3)
    }
}
