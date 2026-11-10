import UIKit

final class SettingsViewController: UIViewController {
    private let root: URL?
    private let surface = GameSurface()
    private let scaleLabel = UILabel()
    private let slider = UISlider()
    private let endpoint = UITextField()
    private var previewGeneration = UUID()
    init(root: URL?) { self.root = root; super.init(nibName: nil, bundle: nil) }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override func viewDidLoad() {
        super.viewDidLoad(); title = "Display and server"; view.backgroundColor = .systemBackground
        slider.minimumValue = 0.5; slider.maximumValue = 1
        let saved = UserDefaults.standard.double(forKey: "displayFraction")
        slider.value = Float(saved >= 0.5 && saved <= 1 ? saved : 1)
        slider.addTarget(self, action: #selector(scaleChanged), for: .valueChanged)
        let reset = UIButton(type: .system); reset.setTitle("Reset to 100%", for: .normal); reset.addTarget(self, action: #selector(resetScale), for: .touchUpInside)
        let mode = UISegmentedControl(items: ["Offline", "Remote server", "Embedded service"])
        mode.selectedSegmentIndex = UserDefaults.standard.integer(forKey: "serverMode") == 1 ? 1 : 0
        mode.setEnabled(false, forSegmentAt: 2); mode.addTarget(self, action: #selector(serverChanged(_:)), for: .valueChanged)
        endpoint.placeholder = "10.9.0.19:18083"; endpoint.borderStyle = .roundedRect; endpoint.autocapitalizationType = .none; endpoint.autocorrectionType = .no; endpoint.keyboardType = .URL
        endpoint.text = UserDefaults.standard.string(forKey: "remoteEndpoint"); endpoint.addTarget(self, action: #selector(endpointChanged), for: .editingDidEnd)
        let note = UILabel(); note.numberOfLines = 0; note.font = .preferredFont(forTextStyle: .footnote)
        note.text = "Configuration only; no server connection is made. Embedded service support awaits server source. The preview fits the complete original bitmap; cut edges in the source artwork are preserved."
        let preview = UIButton(type: .system); preview.setTitle("Preview first original texture", for: .normal); preview.addTarget(self, action: #selector(loadPreview), for: .touchUpInside)
        let stack = UIStackView(arrangedSubviews: [scaleLabel, slider, reset, mode, endpoint, note, preview]); stack.axis = .vertical; stack.spacing = 12; stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 12), stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 20), stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -20)])
        if let surface = surface { surface.translatesAutoresizingMaskIntoConstraints = false; view.addSubview(surface); NSLayoutConstraint.activate([surface.topAnchor.constraint(equalTo: stack.bottomAnchor, constant: 12), surface.leadingAnchor.constraint(equalTo: view.leadingAnchor), surface.trailingAnchor.constraint(equalTo: view.trailingAnchor), surface.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)]) }
        scaleChanged()
    }
    override func viewWillDisappear(_ animated: Bool) { super.viewWillDisappear(animated); previewGeneration = UUID(); surface?.resetTouches(); endpoint.endEditing(true) }
    @objc private func scaleChanged() { scaleLabel.text = "Display size: \(Int(slider.value * 100))% · fixed 768 x 1360"; surface?.fraction = Double(slider.value); UserDefaults.standard.set(Double(slider.value), forKey: "displayFraction") }
    @objc private func resetScale() { slider.value = 1; scaleChanged() }
    @objc private func serverChanged(_ sender: UISegmentedControl) { UserDefaults.standard.set(sender.selectedSegmentIndex == 1 ? 1 : 0, forKey: "serverMode") }
    @objc private func endpointChanged() { UserDefaults.standard.set(endpoint.text, forKey: "remoteEndpoint") }
    @objc private func loadPreview() {
        guard let root = root else { return }; let generation = UUID(); previewGeneration = generation
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            do {
                let directory = root.appendingPathComponent("data/graphics_characard")
                let candidates = try FileManager.default.contentsOfDirectory(at: directory, includingPropertiesForKeys: nil).filter { $0.pathExtension.lowercased() == "ifs" }.sorted { $0.lastPathComponent < $1.lastPathComponent }
                guard let first = candidates.first else { return }; let image = try RBBridge.firstTexture(at: first)
                DispatchQueue.main.async { guard let self = self, self.previewGeneration == generation else { return }; self.surface?.showOriginalImage(image) }
            } catch { DispatchQueue.main.async { guard let self = self, self.previewGeneration == generation else { return }; let a = UIAlertController(title: "Texture could not be loaded", message: error.localizedDescription, preferredStyle: .alert); a.addAction(UIAlertAction(title: "Close", style: .default)); self.present(a, animated: true) } }
        }
    }
}
