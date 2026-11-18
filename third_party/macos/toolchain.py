#!/usr/bin/env python3
"""Identify the selected Xcode compiler and SDK, even when their paths persist."""
import json
from pathlib import Path
import subprocess


def toolchain_info():
    def output(*args, optional=False):
        try:
            return subprocess.check_output(
                args, text=True, stderr=subprocess.DEVNULL if optional else None
            ).strip()
        except (subprocess.CalledProcessError, OSError):
            if not optional:
                raise
            return None

    cc = output("xcrun", "--sdk", "macosx", "--find", "clang")
    return {
        "cc": cc,
        "cxx": output("xcrun", "--sdk", "macosx", "--find", "clang++"),
        "compiler_version": output(cc, "--version"),
        "sdk": str(Path(output("xcrun", "--sdk", "macosx", "--show-sdk-path")).resolve()),
        "sdk_build": output("xcrun", "--sdk", "macosx", "--show-sdk-build-version"),
        # Command Line Tools also provide clang/SDKs, without full Xcode.
        "xcode": output("xcodebuild", "-version", optional=True),
    }


if __name__ == "__main__":
    print(json.dumps(toolchain_info(), sort_keys=True))
