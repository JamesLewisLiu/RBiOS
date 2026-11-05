import XCTest
import MetalKit
import UIKit
@testable import RBiOS

final class GameSurfaceRenderTests: XCTestCase {
    func testFixedSceneRendersCornersAndJudgmentLinesAtEveryDisplayScale() throws {
        guard let device = MTLCreateSystemDefaultDevice() else { throw XCTSkip("Metal is unavailable on this simulator") }
        let queue = try XCTUnwrap(device.makeCommandQueue())
        let surface = try XCTUnwrap(GameSurface(device: device))
        surface.frame = CGRect(x: 0, y: 0, width: 1024, height: 1366)

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
        let draws: [[String: Any]] = points.enumerated().map { index, point in
            let corners = [CGPoint(x: point.x - 16, y: point.y + 16), CGPoint(x: point.x + 16, y: point.y + 16),
                           CGPoint(x: point.x - 16, y: point.y - 16), CGPoint(x: point.x + 16, y: point.y - 16)]
            return ["id": "marker-\(index)", "name": "fixture-white", "image": image,
                    "quad": corners.map { NSValue(cgPoint: $0) }]
        }
        surface.showSessionSprites(draws)
        var pixels = [UInt8](repeating: 0, count: 768 * 1360 * 4)
        var renderedFrames = 0
        for fraction in [0.5, 0.75, 1.0] {
            surface.fraction = fraction; surface.layoutIfNeeded()
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
            let emptyOffset = (400 * 768 + 200) * 4
            XCTAssertLessThan(pixels[emptyOffset], 10, "Markers unexpectedly filled the playfield")
        }
    }
}
