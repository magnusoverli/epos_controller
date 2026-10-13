#!/usr/bin/env python3
"""Seed libFuzzer with the independent capture and structured boundary cases."""
from pathlib import Path
import re
import sys

destination = Path(sys.argv[1])
destination.mkdir(parents=True, exist_ok=True)
header = Path(__file__).resolve().parents[1] / "tests/CapturedDescriptor.h"
capture = "".join(re.findall(r'"([0-9a-f]+)"', header.read_text()))
for name, data in {
    "capture": bytes.fromhex(capture),
    "largest-report": bytes.fromhex("750897000001008102"),
    "globals": b"\xa4" * 64 + b"\xb4" * 64,
    "collections": b"\xa1\x01" * 64 + b"\xc0" * 64,
    "full-descriptor": b"\x05\x01" * 4096,
}.items():
    (destination / name).write_bytes(data)
