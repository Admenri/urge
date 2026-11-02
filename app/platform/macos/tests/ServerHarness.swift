import Foundation

@main
struct ServerHarness {
    static func main() throws {
        let roots = CommandLine.arguments.dropFirst().map { URL(fileURLWithPath: $0, isDirectory: true) }
        let server = try LocalServer(webRoot: roots[0], gameRoot: roots[1], saveRoot: roots[2])
        server.start(ready: { url in
            print(url.absoluteString)
            fflush(stdout)
        }, failed: { error in
            fputs("\(error)\n", stderr)
            exit(1)
        })
        withExtendedLifetime(server) { RunLoop.main.run() }
    }
}
