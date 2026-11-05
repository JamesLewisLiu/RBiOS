import AVFoundation
import QuartzCore

struct AudioClockReading {
    let seconds: TimeInterval
    let hostTime: TimeInterval // player render timestamp in the monotonic host clock
}

// Sample positions, rather than wall time, survive pauses and engine restarts.
struct AudioPlaybackClock {
    let sampleRate: Double
    let totalFrames: UInt64
    private(set) var frame: UInt64 = 0
    private(set) var baseFrame: UInt64 = 0
    init(sampleRate: Double, totalFrames: UInt64) {
        self.sampleRate = sampleRate; self.totalFrames = totalFrames
    }
    var seconds: TimeInterval { Double(frame) / sampleRate }
    var duration: TimeInterval { Double(totalFrames) / sampleRate }
    mutating func observe(sampleTime: Int64?) {
        guard let sampleTime = sampleTime, sampleTime >= 0 else { return }
        let remaining = totalFrames - min(baseFrame, totalFrames)
        frame = max(frame, baseFrame + min(UInt64(sampleTime), remaining))
    }
    mutating func restart() { baseFrame = frame }
    mutating func finishPlayback() { frame = totalFrames }
}

private final class AudioEventGate {
    private let lock = NSLock()
    private var valid = true
    var isValid: Bool { lock.lock(); defer { lock.unlock() }; return valid }
    func cancel() { lock.lock(); valid = false; lock.unlock() }
}

enum AudioStreamError: LocalizedError {
    case invalidFormat, emptyAudio, invalidPCM, cancelled
    var errorDescription: String? {
        switch self {
        case .invalidFormat: return "Cannot create a playback channel for this original audio format."
        case .emptyAudio: return "The original audio contains no playable samples."
        case .invalidPCM: return "The original audio decoder returned an invalid block length."
        case .cancelled: return "Audio preparation was cancelled."
        }
    }
}

// All engine/decoder state lives on work. Only small runtime PCM buffers are
// queued; original resources stay unchanged and no decoded data is written.
final class AudioStream {
    private enum State { case idle, preparing, prepared, playing, paused, completed }
    private let work = DispatchQueue(label: "rb.audio.decode", qos: .userInitiated)
    private let engine = AVAudioEngine()
    private let player = AVAudioPlayerNode()
    private var decoder: RBAudioDecoder?
    private var block = 0
    private var generation: UInt64 = 0
    private var format: AVAudioFormat?
    private var clock: AudioPlaybackClock?
    private var state: State = .idle
    private var skipFrames: UInt64 = 0
    private var outstanding = 0
    private var reachedEnd = false
    private var rebuildOnResume = false
    private var pendingOpen: ((Result<TimeInterval, Error>) -> Void)?
    private var observers: [NSObjectProtocol] = []
    private var events = AudioEventGate()
    var onReady: ((TimeInterval) -> Void)?
    var onComplete: (() -> Void)?
    var onError: ((Error) -> Void)?
    // Stage freezes its driver; an interruption never resumes automatically.
    var onInterruption: (() -> Void)?
    init() {
        engine.attach(player)
        let center = NotificationCenter.default
        observers.append(center.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt,
                  AVAudioSession.InterruptionType(rawValue: raw) == .began else { return }
            self?.interrupt()
        })
        observers.append(center.addObserver(forName: .AVAudioEngineConfigurationChange, object: engine, queue: nil) { [weak self] _ in self?.interrupt() })
        observers.append(center.addObserver(forName: AVAudioSession.routeChangeNotification, object: nil, queue: nil) { [weak self] note in
            guard let raw = note.userInfo?[AVAudioSessionRouteChangeReasonKey] as? UInt,
                  AVAudioSession.RouteChangeReason(rawValue: raw) == .oldDeviceUnavailable else { return }
            self?.interrupt()
        })
    }
    deinit {
        for observer in observers { NotificationCenter.default.removeObserver(observer) }
        player.stop(); engine.stop()
    }
    // Main-thread callbacks. autoplay:false prepares at clock zero, allowing the
    // stage to initialize before it explicitly calls resume().
    func open(_ url: URL, autoplay: Bool = true, completion: ((Result<TimeInterval, Error>) -> Void)? = nil) {
        work.async { [weak self] in
            guard let self = self else { return }
            self.reset(); self.state = .preparing; self.pendingOpen = completion
            do {
                let d = try RBAudioDecoder(url: url)
                guard d.sampleRate.isFinite, d.sampleRate > 0,
                      let f = AVAudioFormat(standardFormatWithSampleRate: d.sampleRate, channels: 2) else { throw AudioStreamError.invalidFormat }
                guard d.frames > 0, d.blockCount > 0 else { throw AudioStreamError.emptyAudio }
                self.decoder = d; self.format = f
                self.clock = AudioPlaybackClock(sampleRate: d.sampleRate, totalFrames: d.frames)
                self.engine.disconnectNodeOutput(self.player)
                self.engine.connect(self.player, to: self.engine.mainMixerNode, format: f)
                self.engine.prepare()
                for _ in 0..<3 { try self.fill(self.generation) }
                guard self.outstanding > 0 else { throw AudioStreamError.emptyAudio }
                self.state = .prepared
                if autoplay { try self.start() }
                let duration = self.clock!.duration
                let callback = self.pendingOpen; self.pendingOpen = nil
                let events = self.events
                DispatchQueue.main.async { [weak self] in
                    guard events.isValid else { callback?(.failure(AudioStreamError.cancelled)); return }
                    self?.onReady?(duration); callback?(.success(duration))
                }
            } catch { self.fail(error) }
        }
    }
    private func fill(_ token: UInt64) throws {
        guard token == generation, let decoder = decoder, let format = format else { return }
        var pcm = Data()
        while pcm.count < 32768 && block < decoder.blockCount {
            let decoded = try decoder.decodeBlock(UInt(block)); block += 1
            guard decoded.count % 4 == 0 else { throw AudioStreamError.invalidPCM }
            let available = UInt64(decoded.count / 4), skip = min(skipFrames, UInt64(decoded.count / 4))
            skipFrames -= skip
            if skip < available { pcm.append(decoded.dropFirst(Int(skip) * 4)) }
        }
        reachedEnd = block >= decoder.blockCount
        let frames = pcm.count / 4
        guard frames > 0 else { return }
        guard let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: AVAudioFrameCount(frames)),
              let channels = buffer.floatChannelData else { throw AudioStreamError.invalidFormat }
        buffer.frameLength = AVAudioFrameCount(frames)
        pcm.withUnsafeBytes { (raw: UnsafeRawBufferPointer) in
            for i in 0..<frames {
                for c in 0..<2 {
                    let at = i * 4 + c * 2, value = UInt16(raw[at]) | UInt16(raw[at + 1]) << 8
                    channels[c][i] = Float(Int16(bitPattern: value)) / 32768
                }
            }
        }
        outstanding += 1
        // dataConsumed occurs before actual output. Only played-back buffers
        // can cause completion/settlement; no wall-clock timer predicts EOF.
        player.scheduleBuffer(buffer, completionCallbackType: .dataPlayedBack) { [weak self] _ in
            self?.work.async { [weak self] in
                guard let self = self, token == self.generation else { return }
                self.outstanding -= 1
                do { try self.fill(token); self.finishIfDrained(token) } catch { self.fail(error) }
            }
        }
    }
    private func observeClock() {
        guard state == .playing else { return }
        let time = player.lastRenderTime.flatMap { player.playerTime(forNodeTime: $0) }
        clock?.observe(sampleTime: time?.sampleTime)
    }
    private func finishIfDrained(_ token: UInt64) {
        guard token == generation, state == .playing, reachedEnd, outstanding == 0 else { return }
        guard engine.isRunning else { interruptOnWork(); return }
        // All callbacks were dataPlayedBack. lastRenderTime may become nil after
        // the final buffer, so its timestamp is not needed to recognize EOF.
        // Stop/reset/interruption callbacks carry an invalidated generation.
        observeClock(); clock?.finishPlayback(); state = .completed
        let events = self.events
        DispatchQueue.main.async { [weak self] in if events.isValid { self?.onComplete?() } }
    }
    private func start() throws {
        guard decoder != nil else { return }
        try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
        try AVAudioSession.sharedInstance().setActive(true)
        if !engine.isRunning { try engine.start() }
        player.play(); state = .playing
        finishIfDrained(generation)
    }
    func pause(completion: (() -> Void)? = nil) {
        work.async { [weak self] in
            guard let self = self else { return }
            if self.state == .playing { self.player.pause(); self.observeClock(); self.state = .paused }
            if let completion = completion { DispatchQueue.main.async(execute: completion) }
        }
    }
    func resume(completion: ((Result<Void, Error>) -> Void)? = nil) {
        work.async { [weak self] in
            guard let self = self else { return }
            guard self.state == .prepared || self.state == .paused else {
                if let completion = completion { DispatchQueue.main.async { completion(.success(())) } }; return
            }
            do {
                if self.rebuildOnResume {
                    // Interruption can discard engine buffers. Re-decode and skip
                    // already heard samples on the worker; memory stays bounded.
                    self.generation &+= 1; self.player.stop(); self.engine.stop()
                    self.block = 0; self.outstanding = 0; self.reachedEnd = false
                    self.clock?.restart(); self.skipFrames = self.clock?.frame ?? 0
                    if let format = self.format {
                        self.engine.disconnectNodeOutput(self.player)
                        self.engine.connect(self.player, to: self.engine.mainMixerNode, format: format)
                    }
                    for _ in 0..<3 { try self.fill(self.generation) }
                    self.rebuildOnResume = false
                }
                try self.start()
                if let completion = completion { DispatchQueue.main.async { completion(.success(())) } }
            } catch {
                self.fail(error)
                if let completion = completion { DispatchQueue.main.async { completion(.failure(error)) } }
            }
        }
    }
    func stop() { work.async { [weak self] in self?.reset() } }
    func readClock(_ completion: @escaping (TimeInterval?) -> Void) {
        readClockReading { completion($0?.seconds) }
    }
    func readClockReading(_ completion: @escaping (AudioClockReading?) -> Void) {
        work.async { [weak self] in
            guard let self = self else { DispatchQueue.main.async { completion(nil) }; return }
            if self.state == .playing && !self.engine.isRunning { self.interruptOnWork() }
            var host = CACurrentMediaTime()
            if self.state == .playing, let renderTime = self.player.lastRenderTime,
               let playerTime = self.player.playerTime(forNodeTime: renderTime) {
                self.clock?.observe(sampleTime: playerTime.sampleTime)
                if renderTime.isHostTimeValid { host = AVAudioTime.seconds(forHostTime: renderTime.hostTime) }
            }
            let reading = self.clock.map { AudioClockReading(seconds: $0.seconds, hostTime: host) }
            DispatchQueue.main.async { completion(reading) }
        }
    }
    private func interrupt() { work.async { [weak self] in self?.interruptOnWork() } }
    private func interruptOnWork() {
        guard state == .playing || state == .paused else { return }
        guard !rebuildOnResume else { return }
        player.pause(); observeClock(); state = .paused; rebuildOnResume = true
        generation &+= 1
        let events = self.events
        DispatchQueue.main.async { [weak self] in if events.isValid { self?.onInterruption?() } }
    }
    private func reset() {
        events.cancel(); events = AudioEventGate()
        generation &+= 1; state = .idle; player.stop(); engine.stop()
        decoder = nil; format = nil; clock = nil; block = 0; skipFrames = 0
        outstanding = 0; reachedEnd = false; rebuildOnResume = false
        if let callback = pendingOpen { pendingOpen = nil; DispatchQueue.main.async { callback(.failure(AudioStreamError.cancelled)) } }
    }
    private func fail(_ error: Error) {
        let callback = pendingOpen; pendingOpen = nil; reset()
        let events = self.events
        DispatchQueue.main.async { [weak self] in
            callback?(.failure(error)); if events.isValid { self?.onError?(error) }
        }
    }
}
