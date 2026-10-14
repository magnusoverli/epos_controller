#!/usr/bin/env python3
"""Check maintained C++ with clang-format 21 without changing files."""
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
formatter = os.environ.get("CLANG_FORMAT", "clang-format")
version = subprocess.check_output([formatter, "--version"], text=True)
if "version 21." not in version:
    raise SystemExit("Use clang-format 21 for the repository's formatting baseline")
failed = []
for directory in [root / "app", root / "tests"]:
    for path in sorted([*directory.rglob("*.cpp"), *directory.rglob("*.h")]):
        formatted = subprocess.check_output([formatter, str(path)], text=True)
        if formatted != path.read_text():
            failed.append(str(path.relative_to(root)))
if failed:
    raise SystemExit("Formatting differs: " + ", ".join(failed))
print("C++ formatting matches clang-format 21")
