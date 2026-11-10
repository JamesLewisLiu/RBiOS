import UIKit
import UniformTypeIdentifiers

final class LibraryViewController: UITableViewController, UIDocumentPickerDelegate {
    private var root: URL?
    private var songs: [[String: Any]] = []
    private let io = DispatchQueue(label: "rb.assets", qos: .userInitiated)
    override func viewDidLoad() {
        super.viewDidLoad(); title = "RBiOS · Offline songs"
        navigationItem.leftBarButtonItem = UIBarButtonItem(title: "Original directory", style: .plain, target: self, action: #selector(importRoot))
        navigationItem.rightBarButtonItem = UIBarButtonItem(title: "Settings", style: .plain, target: self, action: #selector(settings))
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "song")
        let status = UILabel(); status.text = RBBridge.migrationStatus(); status.numberOfLines = 0; status.font = .preferredFont(forTextStyle: .footnote)
        status.frame = CGRect(x: 16, y: 0, width: 600, height: 90); tableView.tableHeaderView = status
        if let data = UserDefaults.standard.data(forKey: "resourceBookmark") {
            var stale = false
            if let url = try? URL(resolvingBookmarkData: data, options: [], relativeTo: nil, bookmarkDataIsStale: &stale) { load(url) }
        }
    }
    deinit { root?.stopAccessingSecurityScopedResource() }
    @objc private func importRoot() { let picker = UIDocumentPickerViewController(forOpeningContentTypes: [.folder], asCopy: false); picker.delegate = self; present(picker, animated: true) }
    func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) { if let url = urls.first { load(url) } }
    private func load(_ url: URL) {
        let accessed = url.startAccessingSecurityScopedResource()
        navigationItem.leftBarButtonItem?.isEnabled = false
        io.async { [weak self] in
            do {
                let list = try RBBridge.catalog(at: url)
                let bookmark = try? url.bookmarkData(options: .minimalBookmark, includingResourceValuesForKeys: nil, relativeTo: nil)
                DispatchQueue.main.async {
                    guard let self = self else { if accessed { url.stopAccessingSecurityScopedResource() }; return }
                    self.root?.stopAccessingSecurityScopedResource(); self.root = url; self.songs = list
                    if let bookmark = bookmark { UserDefaults.standard.set(bookmark, forKey: "resourceBookmark") }
                    self.navigationItem.leftBarButtonItem?.isEnabled = true; self.tableView.reloadData()
                    if self.navigationController?.topViewController === self {
                        self.navigationController?.pushViewController(MenuViewController(root: url, songs: list), animated: true)
                    }
                }
            } catch {
                if accessed { url.stopAccessingSecurityScopedResource() }
                DispatchQueue.main.async { self?.navigationItem.leftBarButtonItem?.isEnabled = true; self?.message(error.localizedDescription) }
            }
        }
    }
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int { songs.count }
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil); let song = songs[indexPath.row]
        cell.textLabel?.text = song["title"] as? String; cell.detailTextLabel?.text = song["artist"] as? String; cell.accessoryType = .disclosureIndicator; return cell
    }
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        guard let root = root else { return }
        navigationController?.pushViewController(MenuViewController(root: root, songs: songs, page: .difficulty, song: songs[indexPath.row]), animated: true)
    }
    @objc private func settings() { navigationController?.pushViewController(SettingsViewController(root: root), animated: true) }
    private func message(_ text: String) { let a = UIAlertController(title: "Resource check", message: text, preferredStyle: .alert); a.addAction(UIAlertAction(title: "Close", style: .default)); present(a, animated: true) }
}
