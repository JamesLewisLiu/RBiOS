import XCTest
@testable import RBiOS

final class AudioStreamTests: XCTestCase {
    func testClockUsesSamplesAndClampsAtOriginalLength() {
        var clock = AudioPlaybackClock(sampleRate: 48000, totalFrames: 96000)
        XCTAssertEqual(clock.seconds, 0)
        clock.observe(sampleTime: 24000)
        XCTAssertEqual(clock.seconds, 0.5)
        clock.observe(sampleTime: 100000)
        XCTAssertEqual(clock.seconds, 2)
        XCTAssertEqual(clock.duration, 2)
    }
    func testUnavailableAndOlderTimestampsNeverMoveBackwards() {
        var clock = AudioPlaybackClock(sampleRate: 44100, totalFrames: 441000)
        clock.observe(sampleTime: 44100)
        clock.observe(sampleTime: nil)
        clock.observe(sampleTime: -1)
        clock.observe(sampleTime: 22050)
        XCTAssertEqual(clock.seconds, 1)
    }
    func testRestartPreservesFrozenPosition() {
        var clock = AudioPlaybackClock(sampleRate: 48000, totalFrames: 480000)
        clock.observe(sampleTime: 72000)
        clock.restart()
        XCTAssertEqual(clock.seconds, 1.5)
        clock.observe(sampleTime: 0)
        XCTAssertEqual(clock.seconds, 1.5)
        clock.observe(sampleTime: 24000)
        XCTAssertEqual(clock.seconds, 2)
        clock.restart()
        clock.observe(sampleTime: 48000)
        XCTAssertEqual(clock.seconds, 3)
    }
    func testPlayedBackCompletionUsesExactOriginalFrameCount() {
        var clock = AudioPlaybackClock(sampleRate: 44100, totalFrames: 52921)
        clock.observe(sampleTime: 52000)
        clock.finishPlayback()
        XCTAssertEqual(clock.frame, 52921)
        XCTAssertEqual(clock.seconds, 52921.0 / 44100.0)
        clock.observe(sampleTime: nil)
        XCTAssertEqual(clock.frame, 52921)
    }
    func testOpenFailureClearsClockAndReportsOnMain() {
        let stream = AudioStream()
        let failed = expectation(description: "missing original audio rejected")
        let cleared = expectation(description: "failed stream has no clock")
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        stream.open(url, autoplay: false) { result in
            XCTAssertTrue(Thread.isMainThread)
            if case .success = result { XCTFail("missing original audio must fail") }
            failed.fulfill()
            stream.readClock { seconds in
                XCTAssertTrue(Thread.isMainThread)
                XCTAssertNil(seconds); cleared.fulfill()
            }
        }
        wait(for: [failed, cleared], timeout: 3)
    }
}
