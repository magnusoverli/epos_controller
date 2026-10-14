"""Exercise the real process: parse errors must never depend on a display."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
environment = dict(os.environ, DISPLAY="", WAYLAND_DISPLAY="", QT_QPA_PLATFORM="invalid",
                   QT_QUICK_BACKEND="software")


def run(arguments, expected, error="", cwd=None):
    result = subprocess.run([binary, *arguments], env=environment, cwd=cwd,
                            capture_output=True, text=True, timeout=15)
    codes = expected if isinstance(expected, tuple) else (expected,)
    assert result.returncode in codes, (arguments, result.returncode, result.stdout, result.stderr)
    assert error in result.stderr, (arguments, result.stderr)
    return result


for option in ["--help", "--help-all", "-h", "-help"]:
    assert "--screenshot" in run([option], 0).stdout
for option in ["--version", "-v"]:
    assert "Headset Controller" in run([option], 0).stdout
for arguments, message in [
    (["--page", "invalid"], "Expected --page"),
    (["--page", "4"], "Expected --page"),
    (["--page", "-1"], "Expected --page"),
    (["--page"], "Missing value"),
    (["--bogus"], "Unknown option"),
    (["--", "--help"], "No positional"),
    (["extra"], "No positional"),
    (["--diagnostics", "--list-devices"], "Choose either"),
    (["--diagnostics", "--page", "0"], "cannot be combined"),
    (["--diagnostics", "-platform", "offscreen"], "cannot be combined"),
    (["--screenshot="], "cannot be empty"),
    (["--read-settings", "--diagnostics"], "Choose only one"),
    (["--read-firmware", "--read-settings"], "Choose only one"),
    (["--read-firmware", "--smoke-test"], "cannot be combined"),
    (["--inspect-firmware", "package.zip"], "requires nonempty"),
    (["--firmware-offer", "offer.json"], "require --inspect-firmware"),
    (["--inspect-firmware", "package.zip", "--firmware-offer", "offer.json", "--read-firmware"], "Choose only one"),
    (["--inspect-firmware", "package.zip", "--firmware-offer", "offer.json", "--smoke-test"], "cannot be combined"),
    (["--read-settings", "--page", "2"], "cannot be combined"),
    (["--device", "1-1"], "requires --read-settings"),
    (["--read-settings", "--device="], "cannot be empty"),
]:
    result = run(arguments, 2, message)
    assert not result.stdout

assert isinstance(json.loads(run(["--list-devices"], (0, 3)).stdout), list)
result = run(["--diagnostics"], (0, 3))
diagnostics = json.loads(result.stdout)
assert diagnostics["schemaVersion"] == 1
assert diagnostics["scanStatus"] in ["ready", "partial", "unavailable"]
assert (result.returncode == 3) == (diagnostics["scanStatus"] == "unavailable")
assert isinstance(diagnostics["issues"], list)
# Deliberately select a nonexistent key: CLI tests must never send to real hardware.
result = run(["--read-settings", "--device", "test-no-such-device"], 4)
assert "error" in json.loads(result.stdout)
result = run(["--read-firmware", "--device", "test-no-such-device"], 4)
assert "error" in json.loads(result.stdout)

with tempfile.TemporaryDirectory(prefix="headset-cli-") as directory:
    # These values previously selected QCoreApplication and aborted window creation.
    for filename in ["--help", "--version"]:
        run(["--screenshot", filename, "-platform", "offscreen"], 2, cwd=directory)
    output = Path(directory) / "frame.png"
    run(["--screenshot", str(output), "--smoke-test", "-platform", "offscreen"], 0)
    assert output.read_bytes().startswith(b"\x89PNG")
print("CLI grammar, headless errors, Qt forwarding and screenshot/smoke checks passed")
