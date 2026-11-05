import UIKit
import MetalKit
import SpriteKit

final class GameSurface: UIView, MTKViewDelegate {
    static let logicalSize = CGSize(width: 768, height: 1360)
    private let metal: MTKView
    private let renderer: SKRenderer
    private let queue: MTLCommandQueue
    private let scene = SKScene(size: GameSurface.logicalSize)
    private var contacts: [ObjectIdentifier: (UInt64, CGPoint)] = [:]
    private var nextID: UInt64 = 1
    var onTouchSnapshot: (([(UInt64, CGPoint)]) -> Void)?
    var onTimedTouchSnapshot: (([(UInt64, CGPoint)], TimeInterval) -> Void)?
    var onFrame: (() -> Void)?
    var onSuspend: (() -> Void)?
    private var sprites: [String: SKSpriteNode] = [:]
    private var spriteRoots: [String: SKNode] = [:]
    private var spriteClips: [String: [[CGPoint]]] = [:]
    private var textures: [String: SKTexture] = [:]
    var fraction: Double = 1 { didSet { resetTouches(); setNeedsLayout() } }
    private(set) var displayRect = CGRect.zero
    private(set) var lastDrawableSize = CGSize.zero
    var configuredDrawableSize: CGSize { metal.drawableSize }
    var configuredSceneSize: CGSize { scene.size }

    init?(device: MTLDevice? = MTLCreateSystemDefaultDevice()) {
        guard let device = device, let q = device.makeCommandQueue() else { return nil }
        metal = MTKView(frame: .zero, device: device); renderer = SKRenderer(device: device); queue = q
        super.init(frame: .zero)
        isMultipleTouchEnabled = true; backgroundColor = .black
        metal.isUserInteractionEnabled = false
        metal.autoResizeDrawable = false; metal.drawableSize = CGSize(width: 768, height: 1360)
        metal.preferredFramesPerSecond = 60; metal.colorPixelFormat = .bgra8Unorm
        metal.depthStencilPixelFormat = .depth32Float_stencil8
        metal.delegate = self; addSubview(metal)
        // Keep original game coordinates independent of Retina pixels and view points.
        scene.scaleMode = .aspectFit; scene.anchorPoint = .zero
        scene.backgroundColor = .black; renderer.scene = scene
        NotificationCenter.default.addObserver(self, selector: #selector(background), name: UIApplication.willResignActiveNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(foreground), name: UIApplication.didBecomeActiveNotification, object: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override func layoutSubviews() {
        super.layoutSubviews()
        let rect = RBBridge.displayRect(in: safeAreaLayoutGuide.layoutFrame, fraction: fraction)
        if rect != displayRect { resetTouches(); displayRect = rect }
        metal.frame = rect
        metal.drawableSize = CGSize(width: 768, height: 1360)
    }
    func showOriginalImage(_ image: UIImage) {
        sprites.removeAll(); spriteRoots.removeAll(); spriteClips.removeAll(); textures.removeAll()
        scene.removeAllChildren()
        let texture = SKTexture(image: image); texture.filteringMode = .linear
        let node = SKSpriteNode(texture: texture)
        let fit = min(768 / image.size.width, 1360 / image.size.height)
        node.size = CGSize(width: image.size.width * fit, height: image.size.height * fit)
        node.position = CGPoint(x: 384, y: 680); scene.addChild(node)
    }
    func showSessionSprites(_ draws: [[String: Any]]) {
        var live = Set<String>()
        for (drawIndex, draw) in draws.enumerated() {
            guard let id = draw["id"] as? String, let name = draw["name"] as? String,
                  let image = draw["image"] as? UIImage else { continue }
            live.insert(id)
            let texture: SKTexture
            if let cached = textures[name] { texture = cached }
            else { texture = SKTexture(image: image); texture.filteringMode = .linear; textures[name] = texture }
            let node: SKSpriteNode
            if let cached = sprites[id] { node = cached; node.texture = texture; node.size = texture.size() }
            else { node = SKSpriteNode(texture: texture); sprites[id] = node; spriteRoots[id] = node; scene.addChild(node) }
            func number(_ key: String, _ fallback: Double = 0) -> CGFloat { CGFloat((draw[key] as? NSNumber)?.doubleValue ?? fallback) }
            node.position = CGPoint(x: number("x"), y: 1360 - number("y"))
            let z = -number("layer") + CGFloat(drawIndex) * 0.00001
            node.zRotation = -number("angle")
            node.setScale(number("scale", 1)); node.alpha = number("alpha", 1)
            node.anchorPoint = CGPoint(x: number("anchorX", 0.5), y: 1 - number("anchorY", 0.5))
            node.blendMode = number("blend") == 8 ? .add : .alpha
            if let color = draw["multiply"] as? [NSNumber], color.count == 4 {
                node.color = UIColor(red: CGFloat(color[0].doubleValue), green: CGFloat(color[1].doubleValue), blue: CGFloat(color[2].doubleValue), alpha: 1)
                node.colorBlendFactor = 1; node.alpha *= CGFloat(color[3].doubleValue)
            } else { node.colorBlendFactor = 0 }
            if let quad = draw["quad"] as? [NSValue], quad.count == 4 {
                let corners = quad.map { CGPoint(x: $0.cgPointValue.x, y: 1360 - $0.cgPointValue.y) }
                let xs = corners.map { $0.x }, ys = corners.map { $0.y }
                let left = xs.min()!, right = xs.max()!, bottom = ys.min()!, top = ys.max()!
                let width = right - left, height = top - bottom
                guard width > 0, height > 0 else { node.isHidden = true; continue }
                node.isHidden = false; node.zRotation = 0; node.setScale(1); node.anchorPoint = CGPoint(x: 0.5, y: 0.5)
                node.position = CGPoint(x: (left + right) / 2, y: (bottom + top) / 2); node.size = CGSize(width: width, height: height)
                let positions = corners.map { SIMD2<Float>(Float(($0.x - left) / width), Float(($0.y - bottom) / height)) }
                let sourcePositions: [SIMD2<Float>] = [SIMD2(0, 0), SIMD2(1, 0), SIMD2(0, 1), SIMD2(1, 1)]
                node.warpGeometry = SKWarpGeometryGrid(columns: 1, rows: 1, sourcePositions: sourcePositions, destinationPositions: positions)
                node.subdivisionLevels = 0
            } else { node.isHidden = false; node.warpGeometry = nil }
            let clips = (draw["clips"] as? [[NSValue]] ?? []).map { clip in
                clip.map { CGPoint(x: $0.cgPointValue.x, y: 1360 - $0.cgPointValue.y) }
            }
            if spriteClips[id] != clips {
                spriteRoots[id]?.removeFromParent(); node.removeFromParent()
                var root: SKNode = node
                for clip in clips.reversed() where clip.count == 4 {
                    let path = CGMutablePath()
                    path.move(to: clip[0]); path.addLine(to: clip[1]); path.addLine(to: clip[3]); path.addLine(to: clip[2]); path.closeSubpath()
                    let mask = SKShapeNode(path: path); mask.fillColor = .white; mask.strokeColor = .clear; mask.lineWidth = 0; mask.isAntialiased = false
                    let crop = SKCropNode(); crop.maskNode = mask; crop.addChild(root); root = crop
                }
                spriteRoots[id] = root; spriteClips[id] = clips; scene.addChild(root)
            }
            if let root = spriteRoots[id], root !== node { root.zPosition = z; node.zPosition = 0 }
            else { node.zPosition = z }
        }
        for id in Array(sprites.keys) where !live.contains(id) {
            spriteRoots.removeValue(forKey: id)?.removeFromParent(); sprites.removeValue(forKey: id); spriteClips.removeValue(forKey: id)
        }
    }
    func draw(in view: MTKView) {
        onFrame?()
        guard let drawable = view.currentDrawable, let pass = view.currentRenderPassDescriptor, let command = queue.makeCommandBuffer() else { return }
        lastDrawableSize = CGSize(width: drawable.texture.width, height: drawable.texture.height)
        guard lastDrawableSize == CGSize(width: 768, height: 1360) else { assertionFailure("render resolution changed"); return }
        encodeScene(at: CACurrentMediaTime(), commandBuffer: command, renderPassDescriptor: pass)
        command.present(drawable); command.commit()
    }
    // Shared by the onscreen drawable and the offscreen GPU regression test.
    func encodeScene(at time: TimeInterval, commandBuffer: MTLCommandBuffer, renderPassDescriptor: MTLRenderPassDescriptor) {
        renderer.update(atTime: time)
        renderer.render(withViewport: CGRect(origin: .zero, size: Self.logicalSize), commandBuffer: commandBuffer, renderPassDescriptor: renderPassDescriptor)
    }
    func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}
    @objc private func background() { resetTouches(); onSuspend?(); metal.isPaused = true }
    @objc private func foreground() { metal.isPaused = false }
    private func sendTouches(at time: TimeInterval) {
        let points = Array(contacts.values).sorted { $0.0 < $1.0 }
        onTouchSnapshot?(points); onTimedTouchSnapshot?(points, time)
    }
    func resetTouches() { contacts.removeAll(); sendTouches(at: CACurrentMediaTime()) }
    private func update(_ touches: Set<UITouch>, beginning: Bool) {
        for touch in touches {
            let key = ObjectIdentifier(touch)
            guard let point = RBBridge.gamePoint(touch.location(in: self), display: displayRect)?.cgPointValue else { contacts.removeValue(forKey: key); continue }
            if let old = contacts[key] { contacts[key] = (old.0, point) }
            else if beginning { contacts[key] = (nextID, point); nextID += 1 }
        }
        sendTouches(at: touches.map { $0.timestamp }.max() ?? CACurrentMediaTime())
    }
    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, beginning: true) }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) { update(touches, beginning: false) }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { for t in touches { contacts.removeValue(forKey: ObjectIdentifier(t)) }; sendTouches(at: touches.map { $0.timestamp }.max() ?? CACurrentMediaTime()) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { resetTouches() }
}
