import UIKit

// This controller schedules the migrated C++ session. No scores or profiles are
// saved; the result screen is a view over the current in-memory snapshot.
final class GameViewController: UIViewController {
    private let root: URL
    private let song: [String: Any]
    private let difficulty: Int
    private let worker = DispatchQueue(label: "rb.stage", qos: .userInteractive)
    private let audio = AudioStream()
    private var session: RBGameSession?
    private var surface: GameSurface?
    private let hud = UILabel()
    private let detail = UILabel()
    private let inputTimeline = StageInputTimeline()
    private var loading = true, paused = true, busy = false, finished = false, startingAudio = false
    private var generation = UUID()
    private var tick: Int32 = -150
    private var preRollFrames: Double = 0
    private var previousFrameTime: CFTimeInterval?
    private var audioStarted = false, audioEnded = false
    private var tailSeconds: Double = 0, finalAudioSeconds: Double = 0
    private var hasAppeared = false
    init(root: URL, song: [String: Any], difficulty: Int) {
        self.root = root; self.song = song; self.difficulty = difficulty
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
        if isMovingFromParent { generation = UUID(); audio.stop(); worker.async { [weak self] in self?.session = nil } }
    }
    deinit { audio.stop() }
    private func prepare() {
        guard let index = (song["index"] as? NSNumber)?.uintValue else { fail("The original song index is missing."); return }
        let token = generation
        worker.async { [weak self] in
            guard let self = self else { return }
            do {
                let prepared = try RBGameSession(root: self.root, songIndex: index, difficulty: UInt(self.difficulty))
                let first = try prepared.advance(to: -150, touches: [])
                self.session = prepared
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token else { return }
                    self.apply(first)
                    self.audio.open(prepared.audioURL, autoplay: false) { [weak self] result in
                        guard let self = self, self.generation == token else { return }
                        switch result {
                        case .success: self.loading = false; self.paused = self.view.window == nil; self.previousFrameTime = nil
                        case .failure(let error): self.fail(error.localizedDescription)
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
                var snapshot: [String: Any] = [:]
                for segment in segments { snapshot = try session.advance(to: segment.targetTick, touches: segment.points) }
                DispatchQueue.main.async { [weak self] in guard let self = self, self.generation == token else { return }; self.busy = false; self.apply(snapshot) }
            } catch { DispatchQueue.main.async { [weak self] in guard let self = self, self.generation == token else { return }; self.busy = false; self.fail(error.localizedDescription) } }
        }
    }
    private func apply(_ snapshot: [String: Any]) {
        tick = (snapshot["tick"] as? NSNumber)?.int32Value ?? tick
        surface?.showSessionSprites(snapshot["sprites"] as? [[String: Any]] ?? [])
        let players = snapshot["players"] as? [[String: Any]] ?? []
        if let local = players.first {
            let score = (local["score"] as? NSNumber)?.intValue ?? 0
            let combo = (local["combo"] as? NSNumber)?.intValue ?? 0
            let rate = (local["rate"] as? NSNumber)?.doubleValue ?? 0
            hud.text = String(format: "SCORE %d    COMBO %d    AR %.2f%%", score, combo, rate * 100)
        }
        let pending = snapshot["pendingPresentation"] as? [String] ?? []
        detail.text = pending.isEmpty ? "Offline - session-only scores" : "Presentation still to restore: " + pending.sorted().prefix(3).joined(separator: ", ")
        if (snapshot["complete"] as? NSNumber)?.boolValue == true && !finished {
            finished = true; freeze(); audio.stop()
            let message = players.enumerated().map { side, p in
                let score = (p["score"] as? NSNumber)?.intValue ?? 0, combo = (p["maxCombo"] as? NSNumber)?.intValue ?? 0
                let rate = (p["finalRate"] as? NSNumber)?.doubleValue ?? 0
                return String(format: "%@  %d points - Max combo %d - AR %.2f%%", side == 0 ? "Player" : "CPU", score, combo, rate * 100)
            }.joined(separator: "\n") + "\n\nThese results will not be saved or uploaded."
            let alert = UIAlertController(title: "Stage complete", message: message, preferredStyle: .alert)
            alert.addAction(UIAlertAction(title: "Back to songs", style: .default) { [weak self] _ in self?.navigationController?.popViewController(animated: true) })
            present(alert, animated: true)
        }
    }
    private func freeze() {
        paused = true; previousFrameTime = nil; surface?.resetTouches(); inputTimeline.cancel()
        audio.pause(); worker.async { [weak self] in self?.session?.cancelTouches() }
    }
    @objc private func pauseMenu() {
        guard !loading, !finished else { return }; freeze()
        let alert = UIAlertController(title: "Paused", message: nil, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Resume", style: .default) { [weak self] _ in
            guard let self = self else { return }; self.previousFrameTime = nil
            if self.audioStarted && !self.audioEnded {
                self.audio.resume { [weak self] result in guard let self = self else { return }; switch result { case .success: self.paused = false; case .failure(let error): self.fail(error.localizedDescription) } }
            } else { self.paused = false }
        })
        alert.addAction(UIAlertAction(title: "Display size", style: .default) { [weak self] _ in guard let self = self else { return }; self.navigationController?.pushViewController(SettingsViewController(root: self.root), animated: true) })
        alert.addAction(UIAlertAction(title: "Exit stage", style: .destructive) { [weak self] _ in self?.navigationController?.popViewController(animated: true) })
        present(alert, animated: true)
    }
    private func fail(_ text: String) {
        guard !finished else { return }; loading = false; finished = true; freeze(); audio.stop(); hud.text = "Stage stopped"
        let alert = UIAlertController(title: "Unable to continue", message: text, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "Back to songs", style: .default) { [weak self] _ in self?.navigationController?.popViewController(animated: true) })
        if presentedViewController == nil { present(alert, animated: true) }
    }
}
