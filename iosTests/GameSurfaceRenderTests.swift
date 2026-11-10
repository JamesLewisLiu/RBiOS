import XCTest
import MetalKit
import UIKit
@testable import RBiOS

final class GameSurfaceRenderTests: XCTestCase {
    func testFixedSceneRendersCornersAndJudgmentLinesAtEveryDisplayScale() throws {
        guard let device = MTLCreateSystemDefaultDevice() else { throw XCTSkip("Metal is unavailable on this simulator") }
        let queue = try XCTUnwrap(device.makeCommandQueue())
        let surface = try XCTUnwrap(GameSurface(device: device))
        let previousWindow = UIApplication.shared.windows.first { $0.isKeyWindow }
        let window = UIWindow(frame: UIScreen.main.bounds)
        let host = UIViewController(); window.rootViewController = host; window.makeKeyAndVisible()
        window.layoutIfNeeded()
        surface.frame = host.view.bounds; host.view.addSubview(surface)
        defer { surface.removeFromSuperview(); window.isHidden = true; previousWindow?.makeKey() }

        let colorDescriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .bgra8Unorm, width: 768, height: 1360, mipmapped: false)
        colorDescriptor.storageMode = .shared; colorDescriptor.usage = [.renderTarget, .shaderRead]
        let color = try XCTUnwrap(device.makeTexture(descriptor: colorDescriptor))
        let depthDescriptor = MTLTextureDescriptor.texture2DDescriptor(pixelFormat: .depth32Float_stencil8, width: 768, height: 1360, mipmapped: false)
        depthDescriptor.storageMode = .private; depthDescriptor.usage = .renderTarget
        let depth = try XCTUnwrap(device.makeTexture(descriptor: depthDescriptor))
        let pass = MTLRenderPassDescriptor()
        pass.colorAttachments[0].texture = color
        pass.colorAttachments[0].loadAction = .clear; pass.colorAttachments[0].storeAction = .store
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1)
        pass.depthAttachment.texture = depth; pass.depthAttachment.loadAction = .clear
        pass.depthAttachment.storeAction = .dontCare; pass.depthAttachment.clearDepth = 1
        pass.stencilAttachment.texture = depth; pass.stencilAttachment.loadAction = .clear
        pass.stencilAttachment.storeAction = .dontCare; pass.stencilAttachment.clearStencil = 0

        let format = UIGraphicsImageRendererFormat(); format.scale = 1
        let image = UIGraphicsImageRenderer(size: CGSize(width: 32, height: 32), format: format).image { context in
            UIColor.white.setFill(); context.fill(CGRect(x: 0, y: 0, width: 32, height: 32))
        }
        let points = [CGPoint(x: 20, y: 20), CGPoint(x: 748, y: 20),
                      CGPoint(x: 20, y: 1340), CGPoint(x: 748, y: 1340),
                      CGPoint(x: 384, y: 680), CGPoint(x: 384, y: 204), CGPoint(x: 384, y: 1098)]
        var draws: [[String: Any]] = points.enumerated().map { index, point in
            let corners = [CGPoint(x: point.x - 16, y: point.y + 16), CGPoint(x: point.x + 16, y: point.y + 16),
                           CGPoint(x: point.x - 16, y: point.y - 16), CGPoint(x: point.x + 16, y: point.y - 16)]
            return ["id": "marker-\(index)", "name": "fixture-white", "image": image,
                    "quad": corners.map { NSValue(cgPoint: $0) }]
        }
        let masks = [[CGPoint(x: 300, y: 416), CGPoint(x: 316, y: 416), CGPoint(x: 300, y: 384), CGPoint(x: 316, y: 384)],
                     [CGPoint(x: 284, y: 408), CGPoint(x: 316, y: 408), CGPoint(x: 284, y: 392), CGPoint(x: 316, y: 392)]]
        draws.append(["id": "masked-panel", "name": "fixture-white", "image": image, "x": 300, "y": 400,
                      "clips": masks.map { $0.map { NSValue(cgPoint: $0) } }])
        // Original menu GE2D solid rectangles have no bitmap/texture resource.
        draws.append(["id": "solid-menu-rectangle", "name": "solid", "solidColor": [0, 1, 0, 1],
                      "quad": [CGPoint(x: 80, y: 520), CGPoint(x: 120, y: 520), CGPoint(x: 80, y: 480), CGPoint(x: 120, y: 480)].map { NSValue(cgPoint: $0) }])
        surface.showSessionSprites(draws)
        var pixels = [UInt8](repeating: 0, count: 768 * 1360 * 4)
        var renderedFrames = 0
        for fraction in [0.5, 0.75, 1.0] {
            surface.fraction = fraction; surface.layoutIfNeeded()
            XCTAssertGreaterThan(surface.displayRect.width, 0)
            XCTAssertGreaterThan(surface.displayRect.height, 0)
            // Render more than once so projection changes made by SpriteKit are observed.
            for _ in 0..<2 {
                let command = try XCTUnwrap(queue.makeCommandBuffer())
                surface.encodeScene(at: Double(renderedFrames) / 60, commandBuffer: command, renderPassDescriptor: pass)
                renderedFrames += 1
                command.commit(); command.waitUntilCompleted()
                XCTAssertEqual(command.status, .completed, String(describing: command.error))
                XCTAssertEqual(surface.configuredSceneSize, GameSurface.logicalSize)
            }
            pixels.withUnsafeMutableBytes { raw in
                color.getBytes(raw.baseAddress!, bytesPerRow: 768 * 4, from: MTLRegionMake2D(0, 0, 768, 1360), mipmapLevel: 0)
            }
            for point in points {
                let offset = (Int(point.y) * 768 + Int(point.x)) * 4
                for channel in 0..<3 {
                    XCTAssertGreaterThan(pixels[offset + channel], 240, "Missing marker at \(point), display fraction \(fraction)")
                }
                let displayPoint = CGPoint(x: surface.displayRect.minX + point.x * surface.displayRect.width / 768,
                                           y: surface.displayRect.minY + point.y * surface.displayRect.height / 1360)
                let mapped = try XCTUnwrap(RBBridge.gamePoint(displayPoint, display: surface.displayRect)).cgPointValue
                XCTAssertEqual(mapped.x, point.x, accuracy: 0.001); XCTAssertEqual(mapped.y, point.y, accuracy: 0.001)
            }
            let solidOffset = (500 * 768 + 100) * 4
            XCTAssertLessThan(pixels[solidOffset], 10)
            XCTAssertGreaterThan(pixels[solidOffset + 1], 240, "Original solid rectangle was not rendered")
            XCTAssertLessThan(pixels[solidOffset + 2], 10)
            let emptyOffset = (400 * 768 + 200) * 4
            XCTAssertLessThan(pixels[emptyOffset], 10, "Markers unexpectedly filled the playfield")
            XCTAssertGreaterThan(pixels[(400 * 768 + 308) * 4], 240, "Mask removed the visible panel")
            XCTAssertLessThan(pixels[(400 * 768 + 292) * 4], 10, "Panel escaped the horizontal mask")
            XCTAssertLessThan(pixels[(412 * 768 + 308) * 4], 10, "Panel escaped the nested vertical mask")
        }
    }
}
