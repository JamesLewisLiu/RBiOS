import UIKit
import MetalKit
import SpriteKit

final class GameSurface: UIView, MTKViewDelegate {
    private let metal: MTKView
    private let renderer: SKRenderer
    private let queue: MTLCommandQueue
    private let scene = SKScene(size: CGSize(width: 768, height: 1360))
    private var contacts: [ObjectIdentifier: (UInt64, CGPoint)] = [:]
    private var nextID: UInt64 = 1
    var onTouchSnapshot: (([(UInt64, CGPoint)]) -> Void)?
    var onTimedTouchSnapshot: (([(UInt64, CGPoint)], TimeInterval) -> Void)?
    var onFrame: (() -> Void)?
    var onSuspend: (() -> Void)?
    private var sprites: [String: SKSpriteNode] = [:]
    private var textures: [String: SKTexture] = [:]
    var fraction: Double = 1 { didSet { resetTouches(); setNeedsLayout() } }
    private(set) var displayRect = CGRect.zero
    private(set) var lastDrawableSize = CGSize.zero
    var configuredDrawableSize: CGSize { metal.drawableSize }

    init?() {
        guard let device = MTLCreateSystemDefaultDevice(), let q = device.makeCommandQueue() else { return nil }
        metal = MTKView(frame: .zero, device: device); renderer = SKRenderer(device: device); queue = q
        super.init(frame: .zero)
        isMultipleTouchEnabled = true; backgroundColor = .black
        metal.isUserInteractionEnabled = false
        metal.autoResizeDrawable = false; metal.drawableSize = CGSize(width: 768, height: 1360)
        metal.preferredFramesPerSecond = 60; metal.colorPixelFormat = .bgra8Unorm
        metal.depthStencilPixelFormat = .depth32Float_stencil8
        metal.delegate = self; addSubview(metal)
        scene.scaleMode = .resizeFill; scene.backgroundColor = .black; renderer.scene = scene
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
        sprites.removeAll(); textures.removeAll()
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
            else { node = SKSpriteNode(texture: texture); sprites[id] = node; scene.addChild(node) }
            func number(_ key: String, _ fallback: Double = 0) -> CGFloat { CGFloat((draw[key] as? NSNumber)?.doubleValue ?? fallback) }
            node.position = CGPoint(x: number("x"), y: 1360 - number("y"))
            node.zPosition = -number("layer") + CGFloat(drawIndex) * 0.00001; node.zRotation = -number("angle")
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
                node.warpGeometry = positions.withUnsafeBufferPointer { SKWarpGeometryGrid(columns: 1, rows: 1, sourcePositions: nil, destinationPositions: $0.baseAddress) }
                node.subdivisionLevels = 0
            } else { node.isHidden = false; node.warpGeometry = nil }
        }
        for id in Array(sprites.keys) where !live.contains(id) { sprites.removeValue(forKey: id)?.removeFromParent() }
    }
    func draw(in view: MTKView) {
        onFrame?()
        guard let drawable = view.currentDrawable, let pass = view.currentRenderPassDescriptor, let command = queue.makeCommandBuffer() else { return }
        lastDrawableSize = CGSize(width: drawable.texture.width, height: drawable.texture.height)
        guard lastDrawableSize == CGSize(width: 768, height: 1360) else { assertionFailure("render resolution changed"); return }
        renderer.update(atTime: CACurrentMediaTime())
        renderer.render(withViewport: CGRect(x: 0, y: 0, width: 768, height: 1360), commandBuffer: command, renderPassDescriptor: pass)
        command.present(drawable); command.commit()
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
