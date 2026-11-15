# Optional Windows console integration regression

Run against a **freshly built Windows GUI engine**, from the repository root:

```sh
python -I test/console-integration/run.py --engine build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe --rgss 1 2 3
```

Python 3.10+ and a usable Windows desktop/GPU backend are required. No external
Ruby, gems, game project, fonts, or development-only engine bindings are needed:
the runner packs the Ruby script using the small Marshal 4.8/zlib format that
RGSS scripts use. Sibling DLLs are copied if the build links shared libraries.
The script works with both poll and fiber hosts; point `--engine` at either build.

Default coverage is RGSS1 and RGSS3. Use `--rgss 1 2 3` for all generations,
`--backend d3d11` (or another compiled backend) to replace the default `opengl`,
and `--case ini-utf16le --case cli-or-false` to select a smaller subset.
`--timeout` defaults to 45 seconds per child and cannot exceed 300 seconds.
The engine requires `Game.ini`; missing console configuration is tested with a
valid game INI that omits the platform section or key.

## What is checked

- PE subsystem is GUI, independently of `CREATE_NO_WINDOW`.
- Missing console section/key and false/0/no/invalid values remain silent;
  true/1/yes and the exact `console` argv token allocate a console.
- Case changes, prefixes, suffixes, switches, and a single quoted
  `"console test"` argument do not match. CLI and INI requests use OR semantics;
  repeated activation does not duplicate startup output.
- `$DEBUG` (RGSS1 `debug`), `$TEST` (RGSS2/3 `test`), and `$BTEST` retain exact-token
  behavior when combined with `console`.
- Unicode titles and installation paths; UTF-8 with/without BOM, UTF-16LE/BE and
  UTF-32LE/BE BOMs. Every authored INI is compared byte-for-byte after shutdown,
  including CLI-only activation with false values and preserved whitespace.
- A tiny RGSS3 archive supplies an authoritative mounted INI that disagrees with
  the loose INI, tested in both directions.
- `GetConsoleWindow`, `GetConsoleProcessList`, all three `GetStdHandle` /
  `GetConsoleMode` results, Ruby fds 0/1/2 and `tty?`, and silent stdin EOF.
- Ruby, direct fd, and WinAPI stdout/stderr writes, including `at_exit` during
  actual interpreter cleanup. An engine logger warning also reaches the console.
- Early engine and SDL system messages appear exactly once after INI activation;
  file logging still works in silent and console modes.

The runner never attaches to a console. It supplies real inherited file handles
and sentinel stdin data, then asserts the engine replaces them with NUL or its
own console. Console output is read **inside the child** through existing
`Win32API`, not captured from the parent's redirected stdout. SDL system startup
logging is enabled deterministically; audio uses SDL's dummy driver.

Artifacts are retained in a new run directory under `build/console-integration`
(or `--output`): `results.json`, each authored game, `Game.log`, parent output
files, `console-results.txt`, and enabled-console screen snapshots. The runner
returns nonzero on failure, kills timed-out children, and terminates its own
child if a native modal dialog appears. Ruby assertions are caught rather than
sent to the host's error dialog; native crash dialogs are disabled explicitly.

## Limits

This is a real desktop integration test, not headless CI or a mocked CMake/unit
test. Console windows can briefly appear for enabled cases. Enabled stdin is
checked for a valid console binding but is never read interactively. Console
screen reads are capped at 131,072 characters; a flooded/scrolled or unusually
small console buffer can cause a diagnostic failure. Driver/font startup must
succeed before the Ruby probe can run. Native dialogs may briefly become visible
before the runner detects and terminates the child; it does not click other apps.
