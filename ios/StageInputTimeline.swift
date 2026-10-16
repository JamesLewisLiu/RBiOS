import Foundation

/// Serial/main-thread adapter from timestamped UIKit snapshots to original 60 Hz ticks.
/// `hostTime` and `clockHostTime` must use the same boot-monotonic seconds as
/// UITouch.timestamp / AVAudioTime.hostTime. The caller freezes/reset this queue
/// while paused; the queue never extrapolates an input backwards into old ticks.
final class StageInputTimeline {
    struct Segment {
        let targetTick: Int32
        let points: [NSValue]
    }
    private struct Event {
        let points: [NSValue]
        let hostTime: TimeInterval
        let order: UInt64
    }
    private var events: [Event] = []
    private var currentPoints: [NSValue] = []
    private var nextOrder: UInt64 = 0

    func enqueue(points: [NSValue], hostTime: TimeInterval) {
        precondition(hostTime.isFinite, "Input timestamp must be finite")
        events.append(Event(points: points, hostTime: hostTime, order: nextOrder))
        nextOrder &+= 1
    }

    /// Each result advances (previous target + 1)...targetTick using its points.
    /// First advance with old points to eventTick-1, then use the new snapshot
    /// beginning at eventTick. Only the last chronological snapshot within one
    /// tick is observable by the game; equal host timestamps use enqueue order.
    func drain(targetTick: Int32, clockSeconds: TimeInterval,
               clockHostTime: TimeInterval, currentTick: Int32) -> [Segment] {
        precondition(clockSeconds.isFinite && clockHostTime.isFinite,
                     "Clock anchor must be finite")
        guard targetTick > currentTick else { return [] }
        let firstTick = currentTick + 1 // safe: targetTick is a larger Int32
        events.sort {
            if $0.hostTime == $1.hostTime { return $0.order < $1.order }
            return $0.hostTime < $1.hostTime
        }
        func effectiveTick(_ event: Event) -> Int32 {
            let value = floor((clockSeconds + (event.hostTime - clockHostTime)) * 60)
            let bounded: Int32
            if value <= Double(Int32.min) { bounded = Int32.min }
            else if value >= Double(Int32.max) { bounded = Int32.max }
            else { bounded = Int32(value) }
            return max(firstTick, bounded)
        }
        var result: [Segment] = []
        var cursor = firstTick
        var consumed = 0
        while consumed < events.count {
            let eventTick = effectiveTick(events[consumed])
            guard eventTick <= targetTick else { break }
            if cursor < eventTick {
                result.append(Segment(targetTick: eventTick - 1, points: currentPoints))
            }
            // Inputs arriving during the same simulation tick collapse to the
            // last snapshot. Events outside the drained range remain queued.
            var last = consumed
            while last + 1 < events.count && effectiveTick(events[last + 1]) == eventTick {
                last += 1
            }
            currentPoints = events[last].points
            consumed = last + 1
            cursor = eventTick
        }
        if consumed > 0 { events.removeFirst(consumed) }
        if cursor <= targetTick {
            result.append(Segment(targetTick: targetTick, points: currentPoints))
        }
        return result
    }

    func reset() {
        events.removeAll(keepingCapacity: true)
        currentPoints = []
        nextOrder = 0
    }

    /// Used together with StageSession.cancelTouches on pause/layout/cancel.
    func cancel() { reset() }
}
