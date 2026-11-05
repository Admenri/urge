"""Package and run the GPU regression with an existing native Game build."""
import argparse
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--game", type=Path, required=True)
parser.add_argument("--ruby", default="ruby")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
stage = root / "build" / "scene3d-regression"
stage.mkdir(parents=True, exist_ok=True)
(stage / "Data").mkdir(exist_ok=True)
shutil.copy2(args.game, stage / "Game.exe")
(stage / "Game.ini").write_text(
    "[Game]\nLibrary=RGSS104E.dll\nScripts=Data\\Scripts.rxdata\n"
    "Title=Scene3D regression\nRTP1=\nRTP2=\nRTP3=\n"
    "[Display]\nBackgroundRunning=true\n", encoding="utf-8"
)
subprocess.run(
    [args.ruby, "-rzlib", "-e",
     "File.binwrite(ARGV[1], Marshal.dump([[0, 'scene3d regression', "
     "Zlib::Deflate.deflate(File.binread(ARGV[0]))]]))",
     str(Path(__file__).with_name("regression.rb")), str(stage / "Data" / "Scripts.rxdata")],
    check=True,
)
result = stage / "scene3d-results.txt"
if result.exists():
    result.unlink()
with (stage / "run.log").open("w", encoding="utf-8") as log:
    subprocess.run([str(stage / "Game.exe")], cwd=stage, stdout=log, stderr=log,
                   check=True, timeout=60, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
report = result.read_text(encoding="utf-8")
print(report, end="")
assert report.count(": OK") == 11 and ": FAIL" not in report, "GPU regression failed"
for name in ("run.log", "Game.log"):
    log = (stage / name).read_text(encoding="utf-8", errors="replace")
    expected_errors = ["dependency cycle", "without an enabled producer",
                       "samples its own", "MRT colors must have matching", "collides"]
    errors = [line for line in log.splitlines() if "[ERROR]" in line]
    assert len(errors) == 5 and all(
        any(message in line for message in expected_errors) for line in errors
    ), log
    assert "Validation Error" not in log, log
