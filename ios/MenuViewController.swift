import UIKit
import AVFoundation

// Original AFP artwork and recovered difficulty rules, with explicit native
// controls for the page logic/fonts/account bindings that remain unported.
final class MenuViewController: UIViewController, UITableViewDataSource, UITableViewDelegate, UISearchBarDelegate {
    enum Page { case title, music, difficulty, result }
    private let root: URL
    private let songs: [[String: Any]]
    private let page: Page
    private let song: [String: Any]?
    private let result: [String: Any]?
    private let resultDifficulty: Int
    private let work = DispatchQueue(label: "rb.menu.assets", qos: .userInitiated)
    private var scene: RBMenuScene?
    private var menuAudio: StageSoundPlayer?
    private var surface: GameSurface?
    private let content = UIView()
    private let status = UILabel()
    private var table: UITableView?
    private var filtered: [[String: Any]] = []
    private var selector: RBDifficultyMenu?
    private var difficultyLabel: UILabel?
    private var detailLabel: UILabel?
    private let inputEpoch = CACurrentMediaTime()
    private var generation = UUID()
    private var visible = false

    init(root: URL, songs: [[String: Any]], page: Page = .title, song: [String: Any]? = nil,
         result: [String: Any]? = nil, difficulty: Int = 0) {
        self.root = root; self.songs = songs; self.page = page; self.song = song
        self.result = result; resultDifficulty = difficulty
        super.init(nibName: nil, bundle: nil)
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override func viewDidLoad() {
        super.viewDidLoad(); view.backgroundColor = .black
        NotificationCenter.default.addObserver(self, selector: #selector(resumeMenuAudio), name: UIApplication.didBecomeActiveNotification, object: nil)
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Settings", style: .plain, target: self, action: #selector(settings))
        content.bounds = CGRect(x: 0, y: 0, width: 768, height: 1360)
        content.layer.anchorPoint = .zero; view.addSubview(content)
        status.frame = CGRect(x: 32, y: 1294, width: 704, height: 56)
        status.textColor = .white; status.numberOfLines = 3; status.font = .systemFont(ofSize: 14)
        status.text = "Loading original menu artwork..."; content.addSubview(status)
        switch page {
        case .title: buildTitle()
        case .music: buildMusic()
        case .difficulty: buildDifficulty()
        case .result: buildResult()
        }
    }
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated); visible = true
        guard let display = GameSurface() else { status.text = "Cannot create the menu renderer."; return }
        surface = display; display.frame = view.bounds; display.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        let fraction = UserDefaults.standard.double(forKey: "displayFraction")
        display.fraction = (0.5...1).contains(fraction) ? fraction : 1
        view.insertSubview(display, belowSubview: content)
        display.useStaticPresentation()
        display.onSuspend = { [weak self] in self?.menuAudio?.pause() }
        // Artwork clocks restart on re-entry. Recovered difficulty state stays in memory.
        let token = generation, name = page == .title ? "title" : page == .result ? "result" : "music"
        if page == .difficulty { prepareMenuAudio(token: token) }
        work.async { [weak self] in
            guard let self = self else { return }
            do {
                let prepared = try RBMenuScene(root: self.root, page: name)
                self.scene = prepared
                let frame = try prepared.frame(at: 0)
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token, self.visible else { return }
                    self.show(frame); self.view.setNeedsLayout()
                }
            } catch { self.reportArtwork(error, token: token) }
        }
    }
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated); visible = false; generation = UUID()
        surface?.onFrame = nil; surface?.removeFromSuperview(); surface = nil
        menuAudio?.stop(); menuAudio = nil
        // Release decoded menu artwork while the game owns the GPU and audio buffers.
        work.async { [weak self] in self?.scene = nil }
    }
    deinit { NotificationCenter.default.removeObserver(self); menuAudio?.stop() }
    @objc private func resumeMenuAudio() {
        guard visible, let player = menuAudio else { return }
        let token = generation
        work.async { [weak self] in
            do {
                try AVAudioSession.sharedInstance().setActive(true)
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.visible, self.generation == token,
                          UIApplication.shared.applicationState == .active else { return }
                    player.resume()
                }
            } catch {
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token else { return }
                    self.status.text = "Menu audio is unavailable: " + error.localizedDescription
                }
            }
        }
    }
    private func playMenuVoice(_ key: String) {
        guard let player = menuAudio else { return }
        let token = generation
        work.async { [weak self] in
            do {
                try AVAudioSession.sharedInstance().setActive(true)
                player.resume { [weak self] result in
                    guard let self = self, self.visible, self.generation == token,
                          UIApplication.shared.applicationState == .active else { player.pause(); return }
                    switch result {
                    case .success: player.play([StageSoundRequest(name: key)])
                    case .failure(let error): self.status.text = "Menu audio is unavailable: " + error.localizedDescription
                    }
                }
            } catch {
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token else { return }
                    self.status.text = "Menu audio is unavailable: " + error.localizedDescription
                }
            }
        }
    }
    private func prepareMenuAudio(token: UUID) {
        let player = StageSoundPlayer(); menuAudio = player
        // The visible menu owns the shared session until the game opens its music.
        work.async { [weak self] in
            guard let self = self else { return }
            do {
                try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
                try AVAudioSession.sharedInstance().setActive(true)
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token, self.visible else { return }
                    player.prepare(RBDifficultyMenu.soundResources(at: self.root)) { [weak self] result in
                        guard let self = self, self.generation == token, self.visible else { player.stop(); return }
                        switch result {
                        case .success:
                            if UIApplication.shared.applicationState == .active { player.resume() }
                        case .failure(let error): self.status.text = "Original menu voice could not be loaded: " + error.localizedDescription
                        }
                    }
                }
            } catch {
                DispatchQueue.main.async { [weak self] in
                    guard let self = self, self.generation == token else { return }
                    self.status.text = "Menu audio is unavailable: " + error.localizedDescription
                }
            }
        }
    }
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews(); surface?.layoutIfNeeded()
        let rect = surface?.displayRect ?? RBBridge.displayRect(in: view.safeAreaLayoutGuide.layoutFrame, fraction: 1)
        content.transform = CGAffineTransform(scaleX: rect.width / 768, y: rect.height / 1360)
        content.layer.position = rect.origin
    }
    private func show(_ frame: [String: Any]) {
        surface?.showSessionSprites(frame["sprites"] as? [[String: Any]] ?? [])
        let pending = frame["pending"] as? [String] ?? []
        let message = pending.isEmpty ? "Offline play - no scores are saved or uploaded" : "Original artwork with native controls. Some original menu elements are still being restored."
        if status.text != message { status.text = message }
    }
    private func reportArtwork(_ error: Error, token: UUID) {
        DispatchQueue.main.async { [weak self] in
            guard let self = self, self.generation == token else { return }
            self.surface?.onFrame = nil
            self.status.text = "Original menu artwork could not be loaded: " + error.localizedDescription
        }
    }
    private func panel(_ rect: CGRect) -> UIView {
        let panel = UIView(frame: rect); panel.backgroundColor = UIColor.black.withAlphaComponent(0.78)
        panel.layer.cornerRadius = 18; content.addSubview(panel); return panel
    }
    private func label(_ text: String, in parent: UIView, frame: CGRect, size: CGFloat = 24) -> UILabel {
        let label = UILabel(frame: frame); label.text = text; label.textColor = .white
        label.numberOfLines = 0; label.textAlignment = .center; label.font = .systemFont(ofSize: size, weight: .medium)
        parent.addSubview(label); return label
    }
    @discardableResult private func button(_ text: String, in parent: UIView, frame: CGRect, action: Selector) -> UIButton {
        let button = UIButton(type: .system); button.frame = frame; button.setTitle(text, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 24, weight: .semibold)
        button.backgroundColor = UIColor.black.withAlphaComponent(0.7); button.layer.cornerRadius = 12
        button.addTarget(self, action: action, for: .touchUpInside); parent.addSubview(button); return button
    }
    private func buildTitle() {
        title = "REFLEC BEAT"
        button("Start offline play", in: content, frame: CGRect(x: 144, y: 1050, width: 480, height: 76), action: #selector(start))
        _ = label("Original resources · \(songs.count) available songs", in: content, frame: CGRect(x: 64, y: 1140, width: 640, height: 54), size: 20)
    }
    @objc private func start() { navigationController?.pushViewController(MenuViewController(root: root, songs: songs, page: .music), animated: true) }
    private func buildMusic() {
        title = "Select music"; filtered = songs
        let search = UISearchBar(frame: CGRect(x: 48, y: 180, width: 672, height: 60))
        search.placeholder = "Find a song or artist"; search.searchBarStyle = .minimal; search.delegate = self
        content.addSubview(search)
        let table = UITableView(frame: CGRect(x: 48, y: 250, width: 672, height: 980), style: .plain)
        table.backgroundColor = UIColor.black.withAlphaComponent(0.78); table.rowHeight = 78
        table.dataSource = self; table.delegate = self; table.keyboardDismissMode = .onDrag
        table.layer.cornerRadius = 16; content.addSubview(table); self.table = table
    }
    func searchBar(_ searchBar: UISearchBar, textDidChange searchText: String) {
        let query = searchText.trimmingCharacters(in: .whitespacesAndNewlines)
        filtered = query.isEmpty ? songs : songs.filter {
            (($0["title"] as? String ?? "") + " " + ($0["artist"] as? String ?? "")).localizedCaseInsensitiveContains(query)
        }; table?.reloadData()
    }
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int { filtered.count }
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "song") ?? UITableViewCell(style: .subtitle, reuseIdentifier: "song")
        let song = filtered[indexPath.row]; cell.textLabel?.text = song["title"] as? String
        cell.detailTextLabel?.text = song["artist"] as? String; cell.textLabel?.textColor = .white
        cell.detailTextLabel?.textColor = .lightGray; cell.backgroundColor = .clear; cell.accessoryType = .disclosureIndicator
        return cell
    }
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true); view.endEditing(true)
        navigationController?.pushViewController(MenuViewController(root: root, songs: songs, page: .difficulty, song: filtered[indexPath.row]), animated: true)
    }
    private func buildDifficulty() {
        title = "Select difficulty"
        guard let song = song, let available = song["difficulties"] as? [NSNumber], let first = available.first else { return }
        do { selector = try RBDifficultyMenu(available: available, initial: first.intValue) }
        catch { status.text = error.localizedDescription; return }
        let area = panel(CGRect(x: 48, y: 330, width: 672, height: 800))
        _ = label(song["title"] as? String ?? "", in: area, frame: CGRect(x: 24, y: 24, width: 624, height: 110), size: 30)
        _ = label(song["artist"] as? String ?? "", in: area, frame: CGRect(x: 24, y: 138, width: 624, height: 70), size: 20)
        difficultyLabel = label("", in: area, frame: CGRect(x: 24, y: 224, width: 624, height: 80), size: 32)
        let basic = button("BASIC", in: area, frame: CGRect(x: 24, y: 324, width: 196, height: 70), action: #selector(basic))
        let medium = button("MEDIUM", in: area, frame: CGRect(x: 238, y: 324, width: 196, height: 70), action: #selector(medium))
        let upper = button("HARD / WHITE", in: area, frame: CGRect(x: 452, y: 324, width: 196, height: 70), action: #selector(upper))
        upper.titleLabel?.font = .systemFont(ofSize: 19, weight: .semibold)
        basic.isEnabled = available.contains(0); medium.isEnabled = available.contains(1)
        upper.isEnabled = available.contains(2) || available.contains(3)
        button("Next difficulty", in: area, frame: CGRect(x: 24, y: 416, width: 300, height: 66), action: #selector(nextDifficulty))
        button("Chart details", in: area, frame: CGRect(x: 348, y: 416, width: 300, height: 66), action: #selector(details))
        detailLabel = label("", in: area, frame: CGRect(x: 24, y: 502, width: 624, height: 96), size: 20)
        button("Play", in: area, frame: CGRect(x: 136, y: 660, width: 400, height: 84), action: #selector(confirm))
        updateDifficulty("idle")
    }
    @objc private func basic() { updateDifficulty("basic") }
    @objc private func medium() { updateDifficulty("medium") }
    @objc private func upper() { updateDifficulty("upper") }
    @objc private func nextDifficulty() { updateDifficulty("next") }
    @objc private func details() { updateDifficulty("detail") }
    @objc private func confirm() { updateDifficulty("confirm") }
    private func updateDifficulty(_ action: String) {
        guard let selector = selector, let song = song else { return }
        do {
            // Absolute monotonic ticks survive navigating away to display settings.
            let tick = Int32(min(Double(Int32.max), floor((CACurrentMediaTime() - inputEpoch) * 60)))
            let state = try selector.update(at: tick, action: action), selected = selector.selected
            let levels = song["levels"] as? [NSNumber] ?? []
            let names = ["BASIC", "MEDIUM", "HARD", "WHITE HARD"]
            difficultyLabel?.text = "\(names[selected]) · Lv.\(selected < levels.count ? levels[selected].intValue : 0)"
            if let voice = state["sound"] as? String, !voice.isEmpty { playMenuVoice(voice) }
            detailLabel?.text = (state["showDetail"] as? NSNumber)?.boolValue == true ? "Original chart · local player versus CPU\nScores stay in this session only" : ""
            if (state["confirmed"] as? NSNumber)?.boolValue == true {
                navigationController?.pushViewController(GameViewController(root: root, song: song, difficulty: selected, menuSongs: songs), animated: true)
            }
        } catch { status.text = error.localizedDescription }
    }
    private func buildResult() {
        title = "Stage result"
        let area = panel(CGRect(x: 48, y: 270, width: 672, height: 940))
        _ = label(song?["title"] as? String ?? "Stage complete", in: area, frame: CGRect(x: 24, y: 22, width: 624, height: 100), size: 30)
        let players = result?["players"] as? [[String: Any]] ?? []
        for (side, player) in players.prefix(2).enumerated() {
            let score = (player["score"] as? NSNumber)?.intValue ?? 0
            let combo = (player["maxCombo"] as? NSNumber)?.intValue ?? 0
            let rate = (player["finalRate"] as? NSNumber)?.doubleValue ?? 0
            let counts = player["counts"] as? [NSNumber] ?? []
            // Grade 3 is a separate internal counter, not a miss or ordinary JUST.
            let countText = [(0, "JUST"), (1, "GREAT"), (2, "GOOD"), (4, "MISS")].map { index, name in
                "\(name): \(index < counts.count ? counts[index].intValue : 0)"
            }.joined(separator: "   ")
            let text = String(format: "%@\n%d points · Max combo %d\nAR %.2f%%", side == 0 ? "Player" : "CPU", score, combo, rate * 100)
            _ = label(text, in: area, frame: CGRect(x: 24, y: 138 + side * 238, width: 624, height: 150), size: 26)
            _ = label(countText, in: area, frame: CGRect(x: 24, y: 290 + side * 238, width: 624, height: 60), size: 17)
        }
        _ = label("These results are not saved or uploaded.", in: area, frame: CGRect(x: 24, y: 646, width: 624, height: 54), size: 20)
        button("Play again", in: area, frame: CGRect(x: 24, y: 744, width: 300, height: 78), action: #selector(retry))
        button("Back to songs", in: area, frame: CGRect(x: 348, y: 744, width: 300, height: 78), action: #selector(backToSongs))
    }
    @objc private func retry() {
        guard let song = song, let navigation = navigationController else { return }
        let game = GameViewController(root: root, song: song, difficulty: resultDifficulty, menuSongs: songs)
        navigation.setViewControllers(Array(navigation.viewControllers.dropLast()) + [game], animated: true)
    }
    @objc private func backToSongs() {
        guard let navigation = navigationController else { return }
        if let music = navigation.viewControllers.first(where: { ($0 as? MenuViewController)?.page == .music }) {
            navigation.popToViewController(music, animated: true)
        } else { navigation.popViewController(animated: true) }
    }
    @objc private func settings() { navigationController?.pushViewController(SettingsViewController(root: root), animated: true) }
}
