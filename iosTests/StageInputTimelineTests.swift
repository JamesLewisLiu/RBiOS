import XCTest
import CoreGraphics
@testable import RBiOS

final class StageInputTimelineTests: XCTestCase {
    private func points(_ identity: Int) -> [NSValue] {
        [NSValue(cgPoint: CGPoint(x: identity, y: identity))]
    }
    private func assertSegments(_ actual: [StageInputTimeline.Segment],
                                _ expected: [(Int32, Int?)],
                                file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(actual.count, expected.count, file: file, line: line)
        for (segment, wanted) in zip(actual, expected) {
            XCTAssertEqual(segment.targetTick, wanted.0, file: file, line: line)
            if let value = wanted.1 {
                XCTAssertEqual(segment.points.count, 1, file: file, line: line)
                XCTAssertEqual(segment.points.first?.cgPointValue.x, CGFloat(value), file: file, line: line)
            } else {
                XCTAssertTrue(segment.points.isEmpty, file: file, line: line)
            }
        }
    }
    func testDelayedFrameKeepsOldInputBeforeActualEventTick() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 99.75)
        assertSegments(timeline.drain(targetTick: 60, clockSeconds: 1,
                                      clockHostTime: 100, currentTick: 30), [(44,nil),(60,1)])
    }
    func testTimestampUsesFloorInsteadOfNearestTick() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 100.0035)
        assertSegments(timeline.drain(targetTick: 70, clockSeconds: 1,
                                      clockHostTime: 100, currentTick: 50), [(59,nil),(70,1)])
    }
    func testPastTimestampClampsToNextUnprocessedTick() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 99)
        assertSegments(timeline.drain(targetTick: 100, clockSeconds: 1,
                                      clockHostTime: 100, currentTick: 90), [(100,1)])
    }
    func testFutureTimestampRemainsQueuedAcrossDrains() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 100.5)
        assertSegments(timeline.drain(targetTick: 60, clockSeconds: 1,
                                      clockHostTime: 100, currentTick: 30), [(60,nil)])
        assertSegments(timeline.drain(targetTick: 100, clockSeconds: 1.5,
                                      clockHostTime: 100.5, currentTick: 60), [(89,nil),(100,1)])
    }
    func testSameTickUsesLastChronologicalSnapshot() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(3), hostTime: 100.109)
        timeline.enqueue(points: points(1), hostTime: 100.101)
        timeline.enqueue(points: points(2), hostTime: 100.105)
        assertSegments(timeline.drain(targetTick: 10, clockSeconds: 0,
                                      clockHostTime: 100, currentTick: 0), [(5,nil),(10,3)])
    }
    func testEqualTimestampUsesLastEnqueuedSnapshotIncludingRelease() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 100.25)
        timeline.enqueue(points: [], hostTime: 100.25)
        assertSegments(timeline.drain(targetTick: 20, clockSeconds: 0,
                                      clockHostTime: 100, currentTick: 0), [(14,nil),(20,nil)])
    }
    func testOutOfOrderDeliveryPreservesTimestampOrder() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(2), hostTime: 100.5)
        timeline.enqueue(points: points(1), hostTime: 100.25)
        assertSegments(timeline.drain(targetTick: 40, clockSeconds: 0,
                                      clockHostTime: 100, currentTick: 0), [(14,nil),(29,1),(40,2)])
    }
    func testPrerollSupportsNegativeOriginalTicks() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 100.25)
        assertSegments(timeline.drain(targetTick: -120, clockSeconds: -2.5,
                                      clockHostTime: 100, currentTick: -151), [(-136,nil),(-120,1)])
    }
    func testResetAndCancelClearHeldSnapshotAndFutureEvents() {
        for shouldCancel in [false,true] {
            let timeline = StageInputTimeline()
            timeline.enqueue(points: points(1), hostTime: 100.25)
            _ = timeline.drain(targetTick: 20, clockSeconds: 0,
                               clockHostTime: 100, currentTick: 0)
            timeline.enqueue(points: points(2), hostTime: 101)
            if shouldCancel { timeline.cancel() } else { timeline.reset() }
            assertSegments(timeline.drain(targetTick: 100, clockSeconds: 0,
                                          clockHostTime: 100, currentTick: 20), [(100,nil)])
        }
    }
    func testNoProgressDoesNotConsumeEvents() {
        let timeline = StageInputTimeline()
        timeline.enqueue(points: points(1), hostTime: 100.25)
        XCTAssertTrue(timeline.drain(targetTick: 10, clockSeconds: 0,
                                    clockHostTime: 100, currentTick: 10).isEmpty)
        assertSegments(timeline.drain(targetTick: 20, clockSeconds: 0,
                                      clockHostTime: 100, currentTick: 10), [(14,nil),(20,1)])
    }
    func testPresentationFrameCadenceDoesNotChangeInputAtAnySimulationTick() {
        func frames(targets: [Int32]) -> [Int] {
            let timeline = StageInputTimeline()
            timeline.enqueue(points: points(1), hostTime: 100.25)
            timeline.enqueue(points: points(2), hostTime: 100.5)
            timeline.enqueue(points: [], hostTime: 100.75)
            var values: [Int] = [], current: Int32 = 0
            for target in targets {
                for segment in timeline.drain(targetTick: target, clockSeconds: 0,
                                               clockHostTime: 100, currentTick: current) {
                    let marker = segment.points.first.map { Int($0.cgPointValue.x) } ?? 0
                    values.append(contentsOf: repeatElement(marker, count: Int(segment.targetTick-current)))
                    current = segment.targetTick
                }
            }
            return values
        }
        XCTAssertEqual(frames(targets: [60]), frames(targets: [10,20,30,40,50,60]))
        XCTAssertEqual(frames(targets: [60]), Array(repeating: 0,count:14) +
                       Array(repeating: 1,count:15) + Array(repeating: 2,count:15) + Array(repeating: 0,count:16))
    }
}
