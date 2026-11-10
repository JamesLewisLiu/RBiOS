import Foundation
import CoreGraphics

// Presentation only: no prediction, clock advancement or input/judgment changes.
struct StageFrameInterpolation {
    private var latest: [[String: Any]] = []
    private var previousByID: [String: [CGPoint]] = [:]
    private var tick: Int32?
    private var receivedAt: TimeInterval = 0
    private var lastFraction: Double = -1
    private var dirty = false

    mutating func submit(_ draws: [[String: Any]], tick newTick: Int32, at time: TimeInterval) {
        guard time.isFinite else { return }
        if let tick = tick, newTick <= tick { return }
        previousByID.removeAll(keepingCapacity: true)
        if let tick = tick, Int64(newTick) - Int64(tick) == 1 {
            let old = Dictionary(latest.compactMap { draw -> (String, [String: Any])? in
                guard let id = draw["id"] as? String else { return nil }; return (id, draw)
            }, uniquingKeysWith: { _, last in last })
            for draw in draws {
                guard let id = draw["id"] as? String, id.first?.isNumber == true,
                      let before = old[id], before["name"] as? String == draw["name"] as? String,
                      (before["layer"] as? NSNumber) == (draw["layer"] as? NSNumber),
                      (before["clips"] as? [[NSValue]] ?? []) == (draw["clips"] as? [[NSValue]] ?? []),
                      let a = before["quad"] as? [NSValue], let b = draw["quad"] as? [NSValue],
                      a.count == 4, b.count == 4 else { continue }
                let from = a.map { $0.cgPointValue }, to = b.map { $0.cgPointValue }
                // Large discontinuities are teleports/redirects rather than in-between positions.
                guard zip(from, to).allSatisfy({ a, b in
                    a.x.isFinite && a.y.isFinite && b.x.isFinite && b.y.isFinite &&
                    abs(a.x - b.x) <= 128 && abs(a.y - b.y) <= 128
                }), from != to else { continue }
                previousByID[id] = from
            }
        }
        latest = draws; tick = newTick; receivedAt = time; lastFraction = -1; dirty = true
    }

    mutating func frame(at time: TimeInterval) -> [[String: Any]]? {
        guard tick != nil, time.isFinite else { return nil }
        let fraction = previousByID.isEmpty ? 1 : min(1, max(0, (time - receivedAt) * 60))
        guard dirty || fraction != lastFraction else { return nil }
        dirty = false; lastFraction = fraction
        if fraction == 1 { previousByID.removeAll(keepingCapacity: true); return latest }
        return latest.map { draw in
            guard let id = draw["id"] as? String, let a = previousByID[id],
                  let b = draw["quad"] as? [NSValue] else { return draw }
            var result = draw
            result["quad"] = zip(a, b).map { start, end in
                NSValue(cgPoint: CGPoint(x: start.x + (end.cgPointValue.x - start.x) * CGFloat(fraction),
                                        y: start.y + (end.cgPointValue.y - start.y) * CGFloat(fraction)))
            }
            return result
        }
    }

    mutating func reset() { self = StageFrameInterpolation() }
}
