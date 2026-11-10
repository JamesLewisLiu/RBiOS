import AVFoundation

struct StageSoundRequest {
    let name: String
    var volume: Float = 1
    var pan: Float = 0
}

enum StageSoundError: LocalizedError {
    case invalidPCM, tooLarge, cancelled, unknownSound(String)
    var errorDescription: String? {
        switch self {
        case .invalidPCM: return "An original sound effect contains invalid PCM data."
        case .tooLarge: return "Original sound effects exceed the in-memory preparation limit."
        case .cancelled: return "Sound effect preparation was cancelled."
        case .unknownSound(let name): return "Original sound effect was not prepared: \(name)"
        }
    }
}

// Completion from a stolen voice must not release its newer replacement.
struct StageSoundVoicePool {
    private(set) var leases: [UInt64]
    private var next: UInt64 = 0
    init(count: Int) { precondition(count > 0); leases = Array(repeating: 0, count: count) }
    mutating func acquire() -> (Int, UInt64) {
        let slot = leases.firstIndex(of: 0) ?? leases.indices.min { leases[$0] < leases[$1] }!
        next &+= 1; leases[slot] = next
        return (slot, next)
    }
    mutating func release(_ slot: Int, lease: UInt64) {
        if leases[slot] == lease { leases[slot] = 0 }
    }
    mutating func cancel() { leases = Array(repeating: 0, count: leases.count) }
}

struct StageSoundPCM {
    let sampleRate: Double
    let bytes: Data // original decoder's interleaved stereo PCM16, little endian
    static let maximumBytes = 16 * 1024 * 1024
    func makeBuffer() throws -> AVAudioPCMBuffer {
        guard sampleRate.isFinite, (8000...192000).contains(sampleRate), !bytes.isEmpty,
              bytes.count % 4 == 0 else { throw StageSoundError.invalidPCM }
        guard bytes.count <= Self.maximumBytes else { throw StageSoundError.tooLarge }
        guard let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 2),
              let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(bytes.count / 4)),
              let channels = buffer.floatChannelData else { throw StageSoundError.invalidPCM }
        buffer.frameLength = AVAudioFrameCount(bytes.count / 4)
        bytes.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
            for frame in 0..<Int(buffer.frameLength) {
                for channel in 0..<2 {
                    let offset = frame * 4 + channel * 2
                    let word = UInt16(raw[offset]) | UInt16(raw[offset + 1]) << 8
                    channels[channel][frame] = Float(Int16(bitPattern: word)) / 32768
                }
            }
        }
        return buffer
    }
    static func decode(_ url: URL) throws -> StageSoundPCM {
        let decoder = try RBAudioDecoder(url: url)
        guard decoder.frames > 0, decoder.frames <= UInt64(maximumBytes / 4) else { throw StageSoundError.tooLarge }
        var bytes = Data(); bytes.reserveCapacity(Int(decoder.frames) * 4)
        for block in 0..<decoder.blockCount {
            let part = try decoder.decodeBlock(UInt(block))
            guard part.count <= maximumBytes - bytes.count else { throw StageSoundError.tooLarge }
            bytes.append(part)
        }
        guard bytes.count == Int(decoder.frames) * 4 else { throw StageSoundError.invalidPCM }
        return StageSoundPCM(sampleRate: decoder.sampleRate, bytes: bytes)
    }
}

// All buffers are decoded before play, in RAM only. The music stream owns the
// shared AVAudioSession; this engine never changes its category or activation.
final class StageSoundPlayer {
    private final class Clip {
        let buffer: AVAudioPCMBuffer
        let nodes: [AVAudioPlayerNode]
        var voices: StageSoundVoicePool
        init(buffer: AVAudioPCMBuffer, count: Int) {
            self.buffer = buffer; nodes = (0..<count).map { _ in AVAudioPlayerNode() }
            voices = StageSoundVoicePool(count: count)
        }
    }
    private let work = DispatchQueue(label: "rb.sound.effects", qos: .userInteractive)
    private let engine = AVAudioEngine()
    private let decode: (URL) throws -> StageSoundPCM
    private let lock = NSLock()
    private var generation: UInt64 = 0
    private var clips: [String: Clip] = [:]
    private var playing = false
    private var loopName: String?
    private var observers: [NSObjectProtocol] = []
    var onError: ((Error) -> Void)?
    var onInterruption: (() -> Void)?
    init(decode: @escaping (URL) throws -> StageSoundPCM = StageSoundPCM.decode) {
        self.decode = decode
        observers.append(NotificationCenter.default.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: nil) { [weak self] _ in
            self?.work.async { [weak self] in
                guard let self = self, self.playing, !self.engine.isRunning else { return }
                self.pauseOnWork()
                DispatchQueue.main.async { [weak self] in self?.onInterruption?() }
            }
        })
        observers.append(NotificationCenter.default.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  raw == AVAudioSession.InterruptionType.began.rawValue else { return }
            self?.pause()
        })
    }
    deinit { observers.forEach { NotificationCenter.default.removeObserver($0) }; engine.stop() }
    private func token(advance: Bool = false) -> UInt64 {
        lock.lock(); defer { lock.unlock() }
        if advance { generation &+= 1 }; return generation
    }
    private func resetOnWork() {
        playing = false; loopName = nil
        for clip in clips.values { for node in clip.nodes { node.stop(); engine.detach(node) } }
        engine.stop(); clips.removeAll()
    }
    func prepare(_ resources: [String: URL], completion: @escaping (Result<Void, Error>) -> Void) {
        let generation = token(advance: true)
        work.async { [weak self] in
            guard let self = self else { return }
            self.resetOnWork()
            do {
                var bytes = 0
                for name in resources.keys.sorted() {
                    guard self.token() == generation else { throw StageSoundError.cancelled }
                    let pcm = try self.decode(resources[name]!)
                    guard self.token() == generation else { throw StageSoundError.cancelled }
                    bytes += pcm.bytes.count
                    guard bytes <= StageSoundPCM.maximumBytes else { throw StageSoundError.tooLarge }
                    let clip = Clip(buffer: try pcm.makeBuffer(), count: 16)
                    for node in clip.nodes {
                        self.engine.attach(node)
                        self.engine.connect(node, to: self.engine.mainMixerNode, format: clip.buffer.format)
                    }
                    self.clips[name] = clip
                }
                self.engine.prepare()
                DispatchQueue.main.async { completion(self.token() == generation ? .success(()) : .failure(StageSoundError.cancelled)) }
            } catch {
                self.resetOnWork()
                DispatchQueue.main.async { completion(.failure(error)) }
            }
        }
    }
    func resume(completion: ((Result<Void, Error>) -> Void)? = nil) {
        let generation = token()
        work.async { [weak self] in
            guard let self = self, self.token() == generation else { return }
            do {
                if !self.clips.isEmpty && !self.engine.isRunning { try self.engine.start() }
                self.playing = true
                DispatchQueue.main.async { completion?(self.token() == generation ? .success(()) : .failure(StageSoundError.cancelled)) }
            } catch { self.report(error); DispatchQueue.main.async { completion?(.failure(error)) } }
        }
    }
    func play(_ requests: [StageSoundRequest]) {
        let generation = token()
        work.async { [weak self] in
            guard let self = self, self.token() == generation, self.playing else { return }
            for request in requests {
                guard let clip = self.clips[request.name] else { self.report(StageSoundError.unknownSound(request.name)); continue }
                guard request.volume.isFinite, request.pan.isFinite else { self.report(StageSoundError.invalidPCM); continue }
                let (slot, lease) = clip.voices.acquire()
                let node = clip.nodes[slot]
                node.stop(); node.volume = min(1, max(0, request.volume)); node.pan = min(1, max(-1, request.pan))
                node.scheduleBuffer(clip.buffer, completionCallbackType: .dataPlayedBack) { [weak self, weak clip] _ in
                    self?.work.async { [weak self, weak clip] in
                        guard let self = self, self.token() == generation else { return }
                        clip?.voices.release(slot, lease: lease)
                    }
                }
                node.play()
            }
        }
    }
    // One aggregate original held-note voice. Repeated snapshots do not restart it.
    // Pausing stops it; the next evaluated game frame decides whether to restart.
    func setHeldLoop(_ name: String?) {
        let generation = token()
        work.async { [weak self] in
            guard let self = self, self.token() == generation, self.playing else { return }
            guard name != self.loopName else { return }
            if let old = self.loopName { self.clips[old]?.nodes.first?.stop() }
            self.loopName = nil
            guard let name = name else { return }
            guard let clip = self.clips[name], let node = clip.nodes.first else {
                self.report(StageSoundError.unknownSound(name)); return
            }
            node.stop(); node.volume = 1; node.pan = 0
            node.scheduleBuffer(clip.buffer, at: nil, options: .loops)
            node.play(); self.loopName = name
        }
    }
    func pause() {
        work.async { [weak self] in self?.pauseOnWork() }
    }
    private func pauseOnWork() {
        playing = false; loopName = nil
        for clip in clips.values { for node in clip.nodes { node.stop() }; clip.voices.cancel() }
        engine.pause()
    }
    func stop() { _ = token(advance: true); work.async { [weak self] in self?.resetOnWork() } }
    private func report(_ error: Error) {
        let generation = token()
        DispatchQueue.main.async { [weak self] in guard let self = self, self.token() == generation else { return }; self.onError?(error) }
    }
}
