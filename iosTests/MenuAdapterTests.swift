import XCTest
import UIKit
@testable import RBiOS

final class MenuAdapterTests: XCTestCase {
    private func descendants(_ view: UIView) -> [UIView] {
        view.subviews.flatMap { [$0] + descendants($0) }
    }
    func testNativeDifficultyControlsRespectImportedChartAvailability() throws {
        let song: [String: Any] = ["title": "Fixture", "artist": "Artist", "difficulties": [0, 3], "levels": [2, 0, 0, 12]]
        let controller = MenuViewController(root: URL(fileURLWithPath: "/unused"), songs: [song], page: .difficulty, song: song)
        // Loading controls alone does not load artwork or execute any game/server code.
        controller.loadViewIfNeeded()
        let controls = descendants(controller.view).compactMap { $0 as? UIButton }
        let upper = try XCTUnwrap(controls.first { $0.title(for: .normal) == "HARD / WHITE" })
        let medium = try XCTUnwrap(controls.first { $0.title(for: .normal) == "MEDIUM" })
        XCTAssertFalse(medium.isEnabled)
        upper.sendActions(for: .touchUpInside)
        XCTAssertTrue(descendants(controller.view).compactMap { ($0 as? UILabel)?.text }.contains("WHITE HARD · Lv.12"))
    }
    func testResultUsesKnownJudgmentCountersAndSessionOnlyActions() {
        let result: [String: Any] = ["players": [["score": 123, "maxCombo": 20, "finalRate": 0.9, "counts": [11, 2, 3, 4, 5]]]]
        let controller = MenuViewController(root: URL(fileURLWithPath: "/unused"), songs: [], page: .result, result: result)
        controller.loadViewIfNeeded()
        let text = descendants(controller.view).compactMap { ($0 as? UILabel)?.text }.joined(separator: "\n")
        XCTAssertTrue(text.contains("JUST: 11")); XCTAssertTrue(text.contains("MISS: 5"))
        XCTAssertTrue(text.contains("not saved or uploaded"))
        let buttons = descendants(controller.view).compactMap { ($0 as? UIButton)?.title(for: .normal) }
        XCTAssertTrue(buttons.contains("Play again")); XCTAssertTrue(buttons.contains("Back to songs"))
    }
    func testOriginalUpperPanelToggleAndAvailableCycle() throws {
        let menu = try RBDifficultyMenu(available: [0, 2, 3], initial: 0)
        let changed = try menu.update(at: 0, action: "upper"); XCTAssertEqual(menu.selected, 2)
        XCTAssertEqual(changed["sound"] as? String, "sys_vox_hard")
        let unchanged = try menu.update(at: 0, action: "idle")
        XCTAssertEqual(unchanged["sound"] as? String, "")
        _ = try menu.update(at: 1, action: "upper"); XCTAssertEqual(menu.selected, 3)
        _ = try menu.update(at: 2, action: "medium"); XCTAssertEqual(menu.selected, 3)
        _ = try menu.update(at: 3, action: "next"); XCTAssertEqual(menu.selected, 0)
        _ = try menu.update(at: 4, action: "next"); XCTAssertEqual(menu.selected, 0)
        _ = try menu.update(at: 8, action: "next"); XCTAssertEqual(menu.selected, 2)
        let confirmed = try menu.update(at: 9, action: "confirm")
        XCTAssertEqual((confirmed["confirmed"] as? NSNumber)?.boolValue, true)
    }
    func testLongIdleDrainsCooldownAndDetailIsSessionOnly() throws {
        let menu = try RBDifficultyMenu(available: [0, 1], initial: 0)
        _ = try menu.update(at: 0, action: "next")
        _ = try menu.update(at: 100_000, action: "next"); XCTAssertEqual(menu.selected, 0)
        let detail = try menu.update(at: 100_001, action: "detail")
        XCTAssertEqual((detail["showDetail"] as? NSNumber)?.boolValue, true)
        let fresh = try RBDifficultyMenu(available: [0], initial: 0)
        XCTAssertEqual((try fresh.update(at: 0, action: "idle")["showDetail"] as? NSNumber)?.boolValue, false)
    }
    func testInvalidAvailabilityActionAndBackwardsClockFail() throws {
        XCTAssertThrowsError(try RBDifficultyMenu(available: [4], initial: 4))
        XCTAssertThrowsError(try RBDifficultyMenu(available: [], initial: 0))
        let menu = try RBDifficultyMenu(available: [0], initial: 0)
        XCTAssertThrowsError(try menu.update(at: 0, action: "unknown"))
        _ = try menu.update(at: 2, action: "idle")
        XCTAssertThrowsError(try menu.update(at: 1, action: "idle"))
    }
}
