#!/usr/bin/env python3
"""Build a pinned, static macOS dependency prefix without a package manager."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import urllib.request

REPO = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name("dependencies.json")


def run(args, *, cwd=None, env=None):
    print("+", " ".join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=cwd, env=env, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arch", default=platform.machine(), choices=["arm64", "x86_64"])
    parser.add_argument("--target", default="13.3")
    parser.add_argument("--jobs", default=os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "8"))
    args = parser.parse_args()
    base = REPO / "build/macos-deps"
    work = base / f"{args.arch}-{args.target}"
    prefix = work / "prefix"
    prefix.mkdir(parents=True, exist_ok=True)
    sdk = subprocess.check_output(["xcrun", "--sdk", "macosx", "--show-sdk-path"], text=True).strip()
    env = os.environ.copy()
    env.update(CC="/usr/bin/clang", CXX="/usr/bin/clang++", MACOSX_DEPLOYMENT_TARGET=args.target,
               PKG_CONFIG_LIBDIR=str(prefix / "lib/pkgconfig"), PKG_CONFIG_PATH=str(prefix / "lib/pkgconfig"))
    # Environment include/library overrides must not inject unrelated installations.
    for key in ("CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "CPPFLAGS", "LDFLAGS"):
        env.pop(key, None)
    locked = json.loads(MANIFEST.read_text())
    recipe = hashlib.sha256(Path(__file__).read_bytes() + MANIFEST.read_bytes()).hexdigest()

    def source(name):
        spec = locked[name]
        archive = base / "downloads" / (name + ".tar.gz")
        archive.parent.mkdir(parents=True, exist_ok=True)
        if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != spec["sha256"]:
            temporary = archive.with_suffix(".download")
            print("Downloading", spec["url"], flush=True)
            with urllib.request.urlopen(spec["url"], timeout=60) as response, temporary.open("wb") as out:
                shutil.copyfileobj(response, out)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != spec["sha256"]:
                temporary.unlink()
                raise RuntimeError(f"Checksum mismatch: {name}")
            temporary.replace(archive)
        root = base / "sources" / (name + "-" + spec["sha256"][:12])
        if not root.exists():
            with tarfile.open(archive) as tar:
                members = tar.getmembers()
                top = Path(members[0].name).parts[0]
                staging = root.with_name(root.name + "-extract")
                staging.mkdir(parents=True, exist_ok=True)
                tar.extractall(staging, filter="data")
                staging.joinpath(top).rename(root)
                staging.rmdir()
        return root, spec["sha256"]

    def vendored(relative):
        path = REPO / relative
        revision = subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
        return path, revision

    def build(name, src, revision, options, *, subdir="", openssl=False):
        folder = work / name
        folder.mkdir(parents=True, exist_ok=True)
        stamp = folder / ".installed.json"
        signature = dict(revision=revision, options=options, arch=args.arch, target=args.target,
                         sdk=sdk, recipe=recipe)
        artifact = {"zlib": "libz.a", "png": "libpng16.a", "webp": "libwebp.a",
                    "aom": "libaom.a", "avif": "libavif.a", "brotli": "libbrotlidec.a",
                    "zstd": "libzstd.a", "nghttp2": "libnghttp2.a", "openssl": "libssl.a"}[name]
        if (stamp.exists() and json.loads(stamp.read_text()) == signature
                and (prefix / "lib" / artifact).is_file()):
            print(f"{name}: cached", flush=True)
            return
        if openssl:
            target = "darwin64-arm64-cc" if args.arch == "arm64" else "darwin64-x86_64-cc"
            run(["/usr/bin/perl", src / "Configure", target, "no-shared", "no-tests", "no-module",
                 f"--prefix={prefix}", f"--openssldir={prefix / 'ssl'}", "--libdir=lib",
                 f"-mmacosx-version-min={args.target}"], cwd=folder, env=env)
            run(["/usr/bin/make", f"-j{args.jobs}"], cwd=folder, env=env)
            run(["/usr/bin/make", "install_sw"], cwd=folder, env=env)
        else:
            run(["cmake", "-S", src / subdir, "-B", folder, "-G", "Ninja",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}", "-DCMAKE_INSTALL_LIBDIR=lib",
                 f"-DCMAKE_OSX_ARCHITECTURES={args.arch}", f"-DCMAKE_OSX_DEPLOYMENT_TARGET={args.target}",
                 f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_C_COMPILER=/usr/bin/clang", "-DCMAKE_CXX_COMPILER=/usr/bin/clang++",
                 f"-DCMAKE_PREFIX_PATH={prefix}", "-DCMAKE_FIND_FRAMEWORK=LAST", "-DBUILD_SHARED_LIBS=OFF",
                 "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local;/Library/Frameworks/Mono.framework",
                 "-DCMAKE_POLICY_VERSION_MINIMUM=3.5", *options], env=env)
            run(["cmake", "--build", folder, "--parallel", args.jobs], env=env)
            run(["cmake", "--install", folder], env=env)
        licenses = prefix / "share/urge/licenses" / name
        licenses.mkdir(parents=True, exist_ok=True)
        for pattern in ("LICENSE*", "COPYING*", "PATENTS", "AUTHORS"):
            for file in src.glob(pattern):
                if file.is_file(): shutil.copy2(file, licenses / file.name)
        stamp.write_text(json.dumps(signature, sort_keys=True))

    def cmake_dep(name, relative, options):
        src, revision = vendored(relative)
        build(name, src, revision, options)

    cmake_dep("zlib", "third_party/zlib", ["-DZLIB_BUILD_SHARED=OFF", "-DZLIB_BUILD_TESTING=OFF"])
    cmake_dep("png", "third_party/SDL_image/external/libpng",
              ["-DPNG_SHARED=OFF", "-DPNG_FRAMEWORK=OFF", "-DPNG_TESTS=OFF", "-DPNG_TOOLS=OFF", "-DZLIB_USE_STATIC_LIBS=ON"])
    cmake_dep("webp", "third_party/SDL_image/external/libwebp",
              [f"-DWEBP_BUILD_{option}=OFF" for option in ("ANIM_UTILS", "CWEBP", "DWEBP", "GIF2WEBP", "IMG2WEBP", "VWEBP", "WEBPINFO", "WEBPMUX", "EXTRAS")]
              + ["-DWEBP_BUILD_LIBWEBPMUX=ON"])
    cmake_dep("aom", "third_party/SDL_image/external/aom",
              [f"-DENABLE_{option}=OFF" for option in ("DOCS", "EXAMPLES", "TESTDATA", "TESTS", "TOOLS")]
              + ["-DCONFIG_AV1_ENCODER=1", "-DCONFIG_AV1_DECODER=1"])
    cmake_dep("avif", "third_party/SDL_image/external/libavif",
              ["-DAVIF_CODEC_AOM=SYSTEM", "-DAVIF_CODEC_DAV1D=OFF", "-DAVIF_LIBYUV=OFF",
               "-DAVIF_BUILD_APPS=OFF", "-DAVIF_BUILD_TESTS=OFF", "-DAVIF_ZLIBPNG=OFF", "-DAVIF_JPEG=OFF"])
    for name, options, subdir in (
        ("brotli", ["-DBROTLI_DISABLE_TESTS=ON", "-DBROTLI_BUILD_TOOLS=OFF"], ""),
        ("zstd", ["-DZSTD_BUILD_SHARED=OFF", "-DZSTD_BUILD_STATIC=ON", "-DZSTD_BUILD_PROGRAMS=OFF", "-DZSTD_BUILD_TESTS=OFF"], "build/cmake"),
        ("nghttp2", ["-DENABLE_LIB_ONLY=ON", "-DBUILD_SHARED_LIBS=OFF", "-DBUILD_STATIC_LIBS=ON", "-DBUILD_TESTING=OFF"], ""),
    ):
        src, revision = source(name)
        build(name, src, revision, options, subdir=subdir)
    src, revision = source("openssl")
    build("openssl", src, revision, [], openssl=True)
    print("Dependency prefix:", prefix, flush=True)


if __name__ == "__main__":
    main()
