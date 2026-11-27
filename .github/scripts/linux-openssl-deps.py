#!/usr/bin/env python3
"""Build CI's static OpenSSL with the same source pin as the macOS build."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import urllib.request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repository", type=Path)
    parser.add_argument("build_directory", type=Path)
    parser.add_argument("install_prefix", type=Path)
    args = parser.parse_args()
    repo = args.repository.resolve()
    work = args.build_directory.resolve()
    prefix = args.install_prefix.resolve()
    work.mkdir(parents=True, exist_ok=True)
    prefix.mkdir(parents=True, exist_ok=True)
    # OpenSSL's source is platform independent; keep a single version/checksum.
    spec = json.loads((repo / "third_party/macos/dependencies.json").read_text())["openssl"]
    archive = work / "openssl.tar.gz"
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != spec["sha256"]:
        temporary = archive.with_suffix(".download")
        with urllib.request.urlopen(spec["url"], timeout=60) as response, temporary.open("wb") as out:
            shutil.copyfileobj(response, out)
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != spec["sha256"]:
            temporary.unlink()
            raise RuntimeError("OpenSSL source checksum mismatch")
        temporary.replace(archive)
    source = work / ("openssl-" + spec["sha256"][:12])
    if not source.exists():
        staging = work / "extract"
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir()
        with tarfile.open(archive) as tar:
            tar.extractall(staging, filter="data")
        roots = list(staging.iterdir())
        if len(roots) != 1 or not (roots[0] / "Configure").is_file():
            raise RuntimeError("Unexpected OpenSSL archive layout")
        roots[0].rename(source)
        staging.rmdir()
    build = work / "build"
    if build.exists():
        shutil.rmtree(build)
    build.mkdir()

    def run(command):
        print("+", " ".join(map(str, command)), flush=True)
        subprocess.run(list(map(str, command)), cwd=build, check=True)

    # No provider .so files or build-prefix paths may be needed at runtime.
    # The default provider is built in; use the target system's trust store.
    run(["perl", source / "Configure", "linux-x86_64", "no-shared", "no-module", "no-tests",
         f"--prefix={prefix}", "--openssldir=/etc/pki/tls", "--libdir=lib", "-fPIC"])
    run(["make", "-j" + os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "3")])
    run(["make", "install_sw"])
    licenses = prefix / "licenses"
    licenses.mkdir(exist_ok=True)
    shutil.copy2(source / "LICENSE.txt", licenses / "openssl.txt")
    for name in ("libssl.a", "libcrypto.a"):
        if not (prefix / "lib" / name).is_file():
            raise RuntimeError(f"Missing static OpenSSL library: {name}")


if __name__ == "__main__":
    main()
