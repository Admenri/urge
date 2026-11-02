import AppKit
import WebKit
import CryptoKit

final class AppDelegate: NSObject, NSApplicationDelegate, WKScriptMessageHandler, WKNavigationDelegate, WKUIDelegate {
    private var window: NSWindow!
    private var webView: WKWebView!
    private var server: LocalServer?
    private var pageURL: URL?
    private var didShowError = false
    private var terminating = false
    private let smokeTest = CommandLine.arguments.contains("--smoke-test")

    func applicationDidFinishLaunching(_ notification: Notification) {
        makeMenu()
        let configuration = WKWebViewConfiguration()
        configuration.mediaTypesRequiringUserActionForPlayback = []
        configuration.preferences.isElementFullscreenEnabled = true
        configuration.userContentController.add(self, name: "urge")
        configuration.userContentController.addUserScript(WKUserScript(source: """
          for (const level of ['log', 'warn', 'error']) {
            const original = console[level].bind(console);
            console[level] = (...args) => {
              original(...args);
              window.webkit.messageHandlers.urge.postMessage({type: level, text: args.map(String).join(' ')});
            };
          }
          window.addEventListener('error', e => window.webkit.messageHandlers.urge.postMessage({type:'fatal', text:e.message}));
          window.addEventListener('unhandledrejection', e => window.webkit.messageHandlers.urge.postMessage({type:'fatal', text:String(e.reason)}));
          """, injectionTime: .atDocumentStart, forMainFrameOnly: true))
        webView = WKWebView(frame: .zero, configuration: configuration)
        webView.navigationDelegate = self
        webView.uiDelegate = self
        webView.isInspectable = true
        window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 960, height: 640),
                          styleMask: [.titled, .closable, .miniaturizable, .resizable], backing: .buffered, defer: false)
        window.title = "URGE"
        window.minSize = NSSize(width: 400, height: 280)
        window.contentView = webView
        window.center()
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
        if let index = CommandLine.arguments.firstIndex(of: "--game"), index + 1 < CommandLine.arguments.count {
            loadGame(URL(fileURLWithPath: CommandLine.arguments[index + 1], isDirectory: true))
        } else if let demo = Bundle.main.resourceURL?.appendingPathComponent("Demo") {
            loadGame(demo)
        }
    }

    private func makeMenu() {
        let bar = NSMenu()
        let app = NSMenuItem()
        bar.addItem(app)
        let appMenu = NSMenu()
        app.submenu = appMenu
        appMenu.addItem(withTitle: "Quit URGE", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")
        let file = NSMenuItem()
        bar.addItem(file)
        let fileMenu = NSMenu(title: "File")
        file.submenu = fileMenu
        fileMenu.addItem(withTitle: "Open Game…", action: #selector(openGame), keyEquivalent: "o").target = self
        fileMenu.addItem(withTitle: "Reload Game", action: #selector(reloadGame), keyEquivalent: "r").target = self
        let edit = NSMenuItem()
        bar.addItem(edit)
        let editMenu = NSMenu(title: "Edit")
        edit.submenu = editMenu
        for (title, action, key) in [("Cut", "cut:", "x"), ("Copy", "copy:", "c"), ("Paste", "paste:", "v"), ("Select All", "selectAll:", "a")] {
            editMenu.addItem(withTitle: title, action: Selector(action), keyEquivalent: key)
        }
        NSApp.mainMenu = bar
    }

    @objc private func openGame() {
        let panel = NSOpenPanel()
        panel.canChooseFiles = false
        panel.canChooseDirectories = true
        panel.allowsMultipleSelection = false
        panel.message = "Choose the game folder containing Game.ini."
        if panel.runModal() == .OK, let url = panel.url { flush { self.loadGame(url) } }
    }

    @objc private func reloadGame() {
        flush { self.didShowError = false; self.webView.reload() }
    }

    private func flush(_ completion: @escaping () -> Void) {
        webView.callAsyncJavaScript("return window.urgeFlushSaves ? await window.urgeFlushSaves() : true;", arguments: [:], in: nil, in: .page, completionHandler: { result in
            switch result {
            case .success(let value): if (value as? Bool) == true { completion() }
            case .failure: completion() // The page may already have exited or failed to load.
            }
        })
    }

    private func loadGame(_ game: URL) {
        guard FileManager.default.fileExists(atPath: game.appendingPathComponent("Game.ini").path),
              let resources = Bundle.main.resourceURL else {
            fail("The selected folder does not contain Game.ini."); return
        }
        do {
            let canonical = game.resolvingSymlinksInPath()
            let identity = canonical.path.hasPrefix(resources.path + "/") ? "bundled-demo" : canonical.path
            let hash = SHA256.hash(data: Data(identity.utf8)).map { String(format: "%02x", $0) }.joined()
            let support = try FileManager.default.url(for: .applicationSupportDirectory, in: .userDomainMask, appropriateFor: nil, create: true)
            let saves = support.appendingPathComponent("URGE/Saves/\(hash)", isDirectory: true)
            let next = try LocalServer(webRoot: resources.appendingPathComponent("Web"), gameRoot: canonical, saveRoot: saves)
            server?.stop()
            server = next
            didShowError = false
            window.title = "URGE — \(game.lastPathComponent)"
            next.start(ready: { [weak self] url in
                self?.pageURL = url
                self?.webView.load(URLRequest(url: url))
            }, failed: { [weak self] error in self?.fail(error.localizedDescription) })
            print("[Host] Game: \(canonical.path)\n[Host] Saves: \(saves.path)")
        } catch { fail(error.localizedDescription) }
    }

    func userContentController(_ userContentController: WKUserContentController, didReceive message: WKScriptMessage) {
        guard message.frameInfo.isMainFrame, let expected = pageURL,
              message.frameInfo.request.url?.host == expected.host,
              message.frameInfo.request.url?.port == expected.port,
              let body = message.body as? [String: String], let type = body["type"], let text = body["text"] else { return }
        print("[\(type)] \(text)")
        fflush(stdout)
        if type == "fatal" { fail(text) }
        if type == "exit", smokeTest { Darwin.exit(Int32(text) ?? 1) }
    }

    private func fail(_ message: String) {
        fputs("[Host error] \(message)\n", stderr)
        if smokeTest { Darwin.exit(1) }
        guard !didShowError else { return }
        didShowError = true
        let alert = NSAlert()
        alert.messageText = "Unable to run this game"
        alert.informativeText = message
        alert.alertStyle = .warning
        alert.beginSheetModal(for: window)
    }

    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        guard let url = navigationAction.request.url, let expected = pageURL else { decisionHandler(.cancel); return }
        decisionHandler(url.host == expected.host && url.port == expected.port && url.path.hasPrefix(expected.deletingLastPathComponent().path + "/") ? .allow : .cancel)
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { fail(error.localizedDescription) }
    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) { fail("The game process stopped unexpectedly. Use File → Reload Game to restart it.") }
    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String, initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
        fail(message)
        completionHandler()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
    func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
        if terminating { return .terminateNow }
        terminating = true
        flush { NSApp.reply(toApplicationShouldTerminate: true) }
        return .terminateLater
    }
    func applicationWillTerminate(_ notification: Notification) { server?.stop() }
}

@main
struct URGEHost {
    static func main() {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)
        let delegate = AppDelegate()
        app.delegate = delegate
        withExtendedLifetime(delegate) { app.run() }
    }
}
