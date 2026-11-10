import UIKit

// This controller schedules the migrated C++ session. No scores or profiles are
// saved; the result screen is a view over the current in-memory snapshot.
final class GameViewController: UIViewController {
    private let root: URL
    private let song: [String: Any]
    private let difficulty: Int
    private let menuSongs: [[String: Any]]
    private let worker = DispatchQueue(label: "rb.stage", qos: .userInteractive)
    private let audio = AudioStream()
    private let sound = StageSoundPlayer()
    private var session: RBGameSession?
    private var surface: GameSurface?
    private let hud = UILabel()
    private let detail = UILabel()
    private let inputTimeline = StageInputTimeline()
    private var loading = true, paused = true, busy = false, finished = false, startingAudio = false
    private var generation = UUID()
    private var resumeGeneration = UUID()
    private var tick: Int32 = -150
    private var preRollFrames: Double = 0
    private var previousFrameTime: CFTimeInterval?
    private var audioStarted = false, audioEnded = false
    private var tailSeconds: Double = 0, finalAudioSeconds: Double = 0
    private var hasAppeared = false
    init(root: URL, song: [String: Any], difficulty: Int, menuSongs: [[String: Any]] = []) {
        self.root = root; self.song = song; self.difficulty = difficulty; self.menuSongs = menuSongs
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .black
        title = song["title"] as? String
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Pause", style: .plain, target: self, action: #selector(pauseMenu))
        guard let display = GameSurface() else { fail("Cannot create the Metal renderer."); return }
        surface = display; display.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(display)
        NSLayoutConstraint.activate([display.leadingAnchor.constraint(equalTo: view.leadingAnchor), display.trailingAnchor.constraint(equalTo: view.trailingAnchor), display.topAnchor.constraint(equalTo: view.topAnchor), display.bottomAnchor.constraint(equalTo: view.bottomAnchor)])
        for label in [hud, detail] { label.textColor = .white; label.numberOfLines = 0; label.font = .monospacedDigitSystemFont(ofSize: 13, weight: .medium); label.translatesAutoresizingMaskIntoConstraints = false; label.isUserInteractionEnabled = false; view.addSubview(label) }
        NSLayoutConstraint.activate([hud.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 4), hud.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 12), detail.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -4), detail.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 12), detail.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -12)])
        hud.text = "Loading original resources..."; detail.text = "Offline - session-only scores"
        display.onTimedTouchSnapshot = { [weak self] points, time in
            guard let self = self, !self.paused, !self.loading else { return }
            self.inputTimeline.enqueue(points: points.map { NSValue(cgPoint: $0.1) }, hostTime: time)
        }
        display.onFrame = { [weak self] in self?.frame() }
        display.onSuspend = { [weak self] in self?.freeze() }
        sound.onError = { [weak self] error in self?.fail(error.localizedDescription) }
        sound.onInterruption = { [weak self] in self?.freeze() }
        audio.onError = { [weak self] error in self?.fail(error.localizedDescription) }
        audio.onInterruption = { [weak self] in self?.freeze() }
        audio.onComplete = { [weak self] in
            guard let self = self else { return }
            self.audio.readClock { [weak self] value in guard let self = self else { return }; self.finalAudioSeconds = value ?? self.finalAudioSeconds; self.audioEnded = true; self.previousFrameTime = nil }
        }
        prepare()
    }
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        let saved = UserDefaults.standard.double(forKey: "displayFraction")
        surface?.fraction = (0.5...1).contains(saved) ? saved : 1
        if hasAppeared && !loading && !finished { paused = true; hud.text = "Paused - use the top-right button to resume" }
        hasAppeared = true
    }
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated); freeze()
        if isMovingFromParent { generation = UUID(); audio.stop(); sound.stop(); worker.async { [weak self] in self?.session = nil } }
    }
    deinit { audio.stop(); sound.stop() }
    private func prepare() {
        guard let index = (song["index"] as? NSNumber)?.uintValue else { fail("The original song index is missing."); return }
        let token = generation
        worker.async { [weak self] in
            guard let self = self else { return }
            do {
                let prepared = try RBGameSession(root: self.root, songIndex: index, difficulty: UInt(self.difficulty))
                let images = try prepared.prepareStageImages()
                let first = try prepared.advance(to: -150, touches: [])
                self.session = prepared
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token else { return }
                    self.apply(first)
                    self.surface?.prepareSessionTextures(images) { [weak self] in
                        guard let self = self, self.generation == token, !self.finished else { return }
                        self.audio.open(prepared.audioURL, autoplay: false) { [weak self] result in
                            guard let self = self, self.generation == token, !self.finished else { return }
                            switch result {
                            case .success:
                                self.sound.prepare(prepared.soundResources) { [weak self] result in
                                    guard let self = self, self.generation == token, !self.finished else { return }
                                    switch result {
                                    case .success: self.loading = false; self.resumeEffects()
                                    case .failure(let error): self.fail(error.localizedDescription)
                                    }
                                }
                            case .failure(let error): self.fail(error.localizedDescription)
                            }
                        }
                    }
                }
            } catch { DispatchQueue.main.async { [weak self] in guard let self = self, self.generation == token else { return }; self.fail(error.localizedDescription) } }
        }
    }
    private func frame() {
        guard !loading, !paused, !finished, !busy, !startingAudio else { return }
        let now = CACurrentMediaTime(), delta = previousFrameTime.map { max(0, now - $0) } ?? 0
        previousFrameTime = now
        if !audioStarted {
            preRollFrames = min(150, preRollFrames + delta * 60)
            if preRollFrames >= 150 {
                startingAudio = true
                audio.resume { [weak self] result in
                    guard let self = self else { return }; self.startingAudio = false
                    switch result { case .success: self.audioStarted = true; case .failure(let error): self.fail(error.localizedDescription) }
                    // If backgrounding raced with start, keep the prepared clock paused.
                    if self.paused { self.audio.pause() }
                }
            } else { advance(Int32(-150 + floor(preRollFrames)), seconds: preRollFrames / 60 - 2.5, host: now) }
        } else if audioEnded {
            tailSeconds += delta; advance(Int32(floor((finalAudioSeconds + tailSeconds) * 60)), seconds: finalAudioSeconds + tailSeconds, host: now)
        } else {
            busy = true
            audio.readClockReading { [weak self] reading in
                guard let self = self else { return }; self.busy = false
                guard !self.paused, !self.finished, let reading = reading else { return }
                self.advance(Int32(floor(reading.seconds * 60)), seconds: reading.seconds, host: reading.hostTime)
            }
        }
    }
    private func advance(_ target: Int32, seconds: TimeInterval, host: TimeInterval) {
        guard target >= tick, !busy else { return }
        let segments = inputTimeline.drain(targetTick: target, clockSeconds: seconds, clockHostTime: host, currentTick: tick)
        guard !segments.isEmpty else { return }
        busy = true; let token = generation
        worker.async { [weak self] in
            guard let self = self, let session = self.session else { return }
            do {
                for segment in segments { try session.advanceLogic(to: segment.targetTick, touches: segment.points) }
                let snapshot = try session.presentation()
                DispatchQueue.main.async { [weak self] in guard let self = self, self.generation == token else { return }; self.busy = false; self.apply(snapshot) }
            } catch { DispatchQueue.main.async { [weak self] in guard let self = self, self.generation == token else { return }; self.busy = false; self.fail(error.localizedDescription) } }
        }
    }
    private func apply(_ snapshot: [String: Any]) {
        tick = (snapshot["tick"] as? NSNumber)?.int32Value ?? tick
        let sprites = snapshot["sprites"] as? [[String: Any]] ?? []
        if paused || loading { surface?.resetPresentationInterpolation(); surface?.showSessionSprites(sprites) }
        else { surface?.submitSessionSprites(sprites, tick: tick) }
        if !paused && !finished {
            let requests = (snapshot["sounds"] as? [String] ?? []).map { StageSoundRequest(name: $0) }
            sound.play(requests)
            let loop = snapshot["holdSound"] as? String ?? ""
            sound.setHeldLoop(loop.isEmpty ? nil : loop)
        }
        let players = snapshot["players"] as? [[String: Any]] ?? []
        if let local = players.first, loading || tick % 6 == 0 || (snapshot["complete"] as? NSNumber)?.boolValue == true {
            let score = (local["score"] as? NSNumber)?.intValue ?? 0
            let combo = (local["combo"] as? NSNumber)?.intValue ?? 0
            let rate = (local["rate"] as? NSNumber)?.doubleValue ?? 0
            hud.text = String(format: "SCORE %d    COMBO %d    AR %.2f%%", score, combo, rate * 100)
        }
        let pending = snapshot["pendingPresentation"] as? [String] ?? []
        let status = pending.isEmpty ? "Offline - session-only scores" : "Presentation still to restore: " + pending.sorted().prefix(3).joined(separator: ", ")
        if detail.text != status { detail.text = status }
        if (snapshot["complete"] as? NSNumber)?.boolValue == true && !finished {
            finished = true; freeze(); audio.stop(); sound.stop()
            if let navigation = navigationController, navigation.topViewController === self {
                let result = MenuViewController(root: root, songs: menuSongs, page: .result, song: song,
                                                result: ["players": players], difficulty: difficulty)
                navigation.setViewControllers(Array(navigation.viewControllers.dropLast()) + [result], animated: true)
            }
        }
    }
    private func resumeEffects() {
        let token = generation, resumeToken = resumeGeneration
        surface?.resetPresentationInterpolation()
        sound.resume { [weak self] result in
            guard let self = self, self.generation == token, !self.finished else { return }
            guard self.resumeGeneration == resumeToken else { self.sound.pause(); self.audio.pause(); return }
            switch result {
            case .success:
                self.paused = self.view.window == nil || UIApplication.shared.applicationState != .active
                if self.paused { self.sound.pause(); self.audio.pause() }
                self.previousFrameTime = nil
            case .failure(let error): self.fail(error.localizedDescription)
            }
        }
    }
    private func freeze() {
        resumeGeneration = UUID()
        paused = true; previousFrameTime = nil; surface?.resetTouches(); inputTimeline.cancel()
        surface?.resetPresentationInterpolation(); sound.pause()
        audio.pause(); worker.async { [weak self] in self?.session?.cancelTouches() }
    }
    @objc private func pauseMenu() {
        guard !loading, !finished else { return }; freeze()
        let alert = UIAlertController(title: "Paused", message: nil, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Resume", style: .default) { [weak self] _ in
            guard let self = self else { return }; self.previousFrameTime = nil
            if self.audioStarted && !self.audioEnded {
                let resumeToken = self.resumeGeneration
                self.audio.resume { [weak self] result in
                    guard let self = self, !self.finished else { return }
                    guard self.resumeGeneration == resumeToken else { self.audio.pause(); return }
                    switch result { case .success: self.resumeEffects(); case .failure(let error): self.fail(error.localizedDescription) }
                }
            } else { self.resumeEffects() }
        })
        alert.addAction(UIAlertAction(title: "Display size", style: .default) { [weak self] _ in guard let self = self else { return }; self.navigationController?.pushViewController(SettingsViewController(root: self.root), animated: true) })
        alert.addAction(UIAlertAction(title: "Exit stage", style: .destructive) { [weak self] _ in self?.navigationController?.popViewController(animated: true) })
        present(alert, animated: true)
    }
    private func fail(_ text: String) {
        guard !finished else { return }; loading = false; finished = true; freeze(); audio.stop(); sound.stop(); hud.text = "Stage stopped"
        let alert = UIAlertController(title: "Unable to continue", message: text, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Back to songs", style: .default) { [weak self] _ in self?.navigationController?.popViewController(animated: true) })
        if presentedViewController == nil { present(alert, animated: true) }
    }
}
