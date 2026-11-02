import Foundation
import Network

/// Serves only bundled engine files and the explicitly selected game, on loopback.
final class LocalServer {
    private let webRoot: URL
    private let gameRoot: URL
    private let saveRoot: URL
    private let listener: NWListener
    private let queue = DispatchQueue(label: "com.urge.webserver")
    private let token = UUID().uuidString

    init(webRoot: URL, gameRoot: URL, saveRoot: URL) throws {
        self.webRoot = webRoot.resolvingSymlinksInPath()
        self.gameRoot = gameRoot.resolvingSymlinksInPath()
        self.saveRoot = saveRoot.resolvingSymlinksInPath()
        try FileManager.default.createDirectory(at: saveRoot, withIntermediateDirectories: true)
        let parameters = NWParameters.tcp
        parameters.requiredLocalEndpoint = .hostPort(host: "127.0.0.1", port: .any)
        listener = try NWListener(using: parameters)
    }

    func start(ready: @escaping (URL) -> Void, failed: @escaping (Error) -> Void) {
        listener.stateUpdateHandler = { [weak self] state in
            guard let self else { return }
            switch state {
            case .ready:
                if let port = self.listener.port,
                   let url = URL(string: "http://127.0.0.1:\(port.rawValue)/\(self.token)/Game.html") {
                    DispatchQueue.main.async { ready(url) }
                }
            case .failed(let error): DispatchQueue.main.async { failed(error) }
            default: break
            }
        }
        listener.newConnectionHandler = { [weak self] connection in
            connection.start(queue: self?.queue ?? .global())
            self?.receive(connection, buffer: Data())
        }
        listener.start(queue: queue)
    }

    func stop() { listener.cancel() }

    private func receive(_ connection: NWConnection, buffer: Data) {
        connection.receive(minimumIncompleteLength: 1, maximumLength: 64 * 1024) { [weak self] data, _, done, error in
            guard let self else { connection.cancel(); return }
            var buffer = buffer
            if let data { buffer.append(data) }
            guard buffer.count <= 64 * 1024 * 1024 else {
                self.respond(connection, status: "413 Payload Too Large"); return
            }
            if let separator = buffer.range(of: Data("\r\n\r\n".utf8)) {
                guard separator.lowerBound < 16 * 1024,
                      let headers = String(data: buffer[..<separator.lowerBound], encoding: .utf8) else {
                    self.respond(connection, status: "400 Bad Request"); return
                }
                let lines = headers.components(separatedBy: "\r\n")
                let parts = (lines.first ?? "").split(separator: " ")
                guard parts.count == 3 else { self.respond(connection, status: "400 Bad Request"); return }
                var fields: [String: String] = [:]
                for line in lines.dropFirst() {
                    guard let colon = line.firstIndex(of: ":") else { continue }
                    fields[line[..<colon].lowercased()] = line[line.index(after: colon)...].trimmingCharacters(in: .whitespaces)
                }
                guard fields["transfer-encoding"] == nil,
                      let length = Int(fields["content-length"] ?? "0"), length >= 0,
                      length <= 64 * 1024 * 1024 else {
                    self.respond(connection, status: "400 Bad Request"); return
                }
                if buffer.count - separator.upperBound >= length {
                    self.handle(connection, method: String(parts[0]), target: String(parts[1]),
                                fields: fields, body: buffer.subdata(in: separator.upperBound..<(separator.upperBound + length)))
                    return
                }
            } else if buffer.count > 16 * 1024 {
                self.respond(connection, status: "431 Request Header Fields Too Large"); return
            }
            if done || error != nil { connection.cancel() }
            else { self.receive(connection, buffer: buffer) }
        }
    }

    private func file(_ root: URL, _ path: String) -> URL? {
        guard !path.isEmpty, !path.contains("\0"), !path.contains("\\"),
              !path.split(separator: "/").contains("..") else { return nil }
        let url = root.appendingPathComponent(path).resolvingSymlinksInPath()
        guard url.path.hasPrefix(root.path + "/") else { return nil }
        return url
    }

    private func manifest() throws -> Data {
        var entries: [String: [String: Any]] = [:]
        for (root, saved) in [(gameRoot, false), (saveRoot, true)] {
            guard let files = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey, .fileSizeKey], options: [.skipsHiddenFiles]) else { continue }
            for case let url as URL in files {
                let canonical = url.resolvingSymlinksInPath()
                guard canonical.path.hasPrefix(root.path + "/") else { continue }
                let relative = String(canonical.path.dropFirst(root.path.count + 1))
                guard file(root, relative) != nil,
                      let values = try? url.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                      values.isRegularFile == true else { continue }
                let lower = relative.lowercased()
                entries[lower] = ["path": relative, "size": values.fileSize ?? 0,
                                  "preload": saved || lower.hasSuffix(".ini") || lower.hasSuffix(".acs") || lower.hasPrefix("data/scripts.")]
            }
        }
        var result = Data("const asyncMappingData = ".utf8)
        result.append(try JSONSerialization.data(withJSONObject: entries, options: [.sortedKeys]))
        result.append(Data(";\n".utf8))
        return result
    }

    private func handle(_ connection: NWConnection, method: String, target: String, fields: [String: String], body: Data) {
        guard let port = listener.port,
              fields["host"] == "127.0.0.1:\(port.rawValue)",
              let path = String(target.split(separator: "?", maxSplits: 1)[0]).removingPercentEncoding,
              path.hasPrefix("/\(token)/") else { respond(connection, status: "403 Forbidden"); return }
        let relative = String(path.dropFirst(token.count + 2))
        do {
            if relative.hasPrefix("save/"), method == "PUT" {
                guard fields["origin"] == "http://127.0.0.1:\(port.rawValue)",
                      let url = file(saveRoot, String(relative.dropFirst(5))) else {
                    respond(connection, status: "403 Forbidden"); return
                }
                try FileManager.default.createDirectory(at: url.deletingLastPathComponent(), withIntermediateDirectories: true)
                try body.write(to: url, options: .atomic)
                respond(connection, status: "204 No Content")
                return
            }
            guard method == "GET" || method == "HEAD" else { respond(connection, status: "405 Method Not Allowed"); return }
            let data: Data
            var type = "application/octet-stream"
            if relative == "async_mapping.js" {
                data = try manifest()
                type = "text/javascript; charset=utf-8"
            } else {
                let url: URL?
                if relative.hasPrefix("assets/") {
                    let asset = String(relative.dropFirst(7))
                    let saved = file(saveRoot, asset)
                    url = saved.flatMap { FileManager.default.fileExists(atPath: $0.path) ? $0 : nil } ?? file(gameRoot, asset)
                } else { url = file(webRoot, relative) }
                guard let url else { respond(connection, status: "403 Forbidden"); return }
                switch url.pathExtension.lowercased() {
                case "html": type = "text/html; charset=utf-8"
                case "js": type = "text/javascript; charset=utf-8"
                case "wasm": type = "application/wasm"
                case "png": type = "image/png"
                case "ogg": type = "audio/ogg"
                default: break
                }
                var content = try Data(contentsOf: url, options: .mappedIfSafe)
                if relative == "Game.html", let html = String(data: content, encoding: .utf8) {
                    let marker = "<script type='text/javascript' src=\"urge-helper.js\"></script>"
                    guard html.contains(marker) else { throw CocoaError(.fileReadCorruptFile) }
                    content = Data(html.replacingOccurrences(of: marker, with: marker + "\n<script src=\"host.js\"></script>").utf8)
                }
                data = content
            }
            respond(connection, status: "200 OK", type: type, body: data, head: method == "HEAD")
        } catch {
            respond(connection, status: "404 Not Found")
        }
    }

    private func respond(_ connection: NWConnection, status: String, type: String = "text/plain", body: Data = Data(), head: Bool = false) {
        var response = Data("HTTP/1.1 \(status)\r\nContent-Type: \(type)\r\nContent-Length: \(body.count)\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\n\r\n".utf8)
        if !head { response.append(body) }
        connection.send(content: response, completion: .contentProcessed { _ in connection.cancel() })
    }
}
