import UIKit
import UniformTypeIdentifiers

// Resource access and diagnostics are platform UI, never a substitute TEST page.
final class OriginalBootViewController: UIViewController, UIDocumentPickerDelegate {
    private let startup = RBOriginalStartup()
    private let work = DispatchQueue(label: "rb.original-startup", qos: .userInitiated)
    private var resource: URL?
    private var scoped = false
    private let output = UITextView()
    private var running = false

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Original startup"
        view.backgroundColor = .black
        output.isEditable = false
        output.font = .monospacedSystemFont(ofSize: 15, weight: .regular)
        output.textColor = .white
        output.backgroundColor = .black
        output.text = "Select the original game directory to begin startup.\n\nUnrecovered services stop startup. The component harness is not started automatically."
        output.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(output)
        NSLayoutConstraint.activate([
            output.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 16),
            output.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -16),
            output.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 16),
            output.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -16)
        ])
        navigationItem.leftBarButtonItem = UIBarButtonItem(title: "Game directory", style: .plain, target: self, action: #selector(selectDirectory))
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Retry", style: .plain, target: self, action: #selector(retry))
        navigationItem.rightBarButtonItem?.isEnabled = false
        if let bookmark = UserDefaults.standard.data(forKey: "resourceBookmark") {
            var stale = false
            if let url = try? URL(resolvingBookmarkData: bookmark, options: [], relativeTo: nil, bookmarkDataIsStale: &stale) { begin(url) }
        }
    }
    deinit {
        startup.stop()
        if scoped { resource?.stopAccessingSecurityScopedResource() }
    }
    @objc private func selectDirectory() {
        guard !running else { return }
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.folder], asCopy: false)
        picker.delegate = self
        present(picker, animated: true)
    }
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
        if let url = urls.first { begin(url) }
    }
    @objc private func retry() { if let resource = resource { begin(resource) } }
    private func begin(_ url: URL) {
        guard !running else { return }
        running = true
        startup.stop()
        let accessed = url.startAccessingSecurityScopedResource()
        if scoped { resource?.stopAccessingSecurityScopedResource() }
        resource = url; scoped = accessed
        navigationItem.leftBarButtonItem?.isEnabled = false
        navigationItem.rightBarButtonItem?.isEnabled = false
        output.text = "Starting original configuration and AVS initialization..."
        // Strong ownership keeps security-scoped access alive until the worker
        // finishes. The bridge owns a dedicated native runtime thread: a serial
        // GCD queue alone cannot preserve the original thread-local identity.
        work.async { [self] in
            let result: Result<[String: Any], Error>
            do { result = .success(try startup.start(at: url)) }
            catch { result = .failure(error) }
            let bookmark = try? url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
            DispatchQueue.main.async { [self] in
                running = false
                navigationItem.leftBarButtonItem?.isEnabled = true
                navigationItem.rightBarButtonItem?.isEnabled = true
                if let bookmark = bookmark { UserDefaults.standard.set(bookmark, forKey: "resourceBookmark") }
                switch result {
                case .success(let state):
                    let trace = (state["trace"] as? [String] ?? []).joined(separator: "\n")
                    let address = state["blockedAddress"] as? String ?? "Unknown dependency"
                    let reason = state["message"] as? String ?? "Startup has not completed."
                    let status = state["failed"] as? Bool == true ? "Startup failed" : "Startup stopped"
                    output.text = "\(trace)\n\n\(status) at \(address)\n\n\(reason)"
                case .failure(let error): output.text = "Startup failed\n\n\(error.localizedDescription)"
                }
            }
        }
    }
}
