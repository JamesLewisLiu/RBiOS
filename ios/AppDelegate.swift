import UIKit

@main
final class AppDelegate: UIResponder, UIApplicationDelegate {
    var window: UIWindow?
    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        let w = UIWindow(frame: UIScreen.main.bounds)
        let root: UIViewController
#if DEBUG
        root = ProcessInfo.processInfo.arguments.contains("--component-harness")
            ? LibraryViewController() : OriginalBootViewController()
#else
        root = OriginalBootViewController()
#endif
        w.rootViewController = UINavigationController(rootViewController: root)
        w.makeKeyAndVisible(); window = w
        return true
    }
}
