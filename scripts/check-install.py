#!/usr/bin/env python3
"""Stage, relocate, and exercise the installed runtime, isolated from SDK overrides."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="headset-install-") as directory:
    root = Path(directory)
    original = root / "stage"
    installation = subprocess.run(["cmake", "--install", str(args.build.resolve()), "--prefix", str(original)],
                                  capture_output=True, text=True)
    if installation.returncode:
        print(installation.stdout + installation.stderr)
        installation.check_returncode()
    relocated = root / "relocated"
    shutil.move(original, relocated)
    binary = relocated / "bin/headset-controller"
    env = os.environ.copy()
    for key in ["LD_LIBRARY_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH"]:
        env.pop(key, None)
    env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software", DISPLAY="", WAYLAND_DISPLAY="")
    result = subprocess.run([binary, "--diagnostics"], env=env, check=True, capture_output=True, text=True, timeout=15)
    document = json.loads(result.stdout)
    assert document["schemaVersion"] == 1
    libraries = subprocess.check_output(["ldd", binary], env=env, text=True)
    assert "not found" not in libraries, libraries
    for line in libraries.splitlines():
        if "libQt6" in line and "=>" in line:
            assert str(relocated) in line, f"Qt escaped deployed prefix: {line}"
    for page in range(4):
        subprocess.run([binary, "--smoke-test", "--page", str(page)], env=env, check=True, timeout=15)
    subprocess.run([binary, "--smoke-test", "--open-diagnostics"], env=env, check=True, timeout=15)
    print("Relocated install: matching Qt linkage, JSON diagnostics and all GUI pages passed")
