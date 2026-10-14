#!/usr/bin/env python3
"""Verify Linux mute-event captures: verify-mute-evidence.py captures/linux"""
import hashlib
import json
from pathlib import Path
import re
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def crc(body):
    # Polynomial long division, independent of the C++ byte-at-a-time algorithm.
    remainder = (int.from_bytes(body, "big") << 8) ^ (255 << (8 * len(body)))
    for bit in range(remainder.bit_length() - 1, 7, -1):
        if remainder & (1 << bit):
            remainder ^= 0x107 << (bit - 8)
    return remainder


def main(root):
    observed = set()
    count = 0
    for directory, expected_hash, expected_reports in [
        ("20261014T085946Z-mute-events", "06b18f15753d6450a76094251692a5aabe12d3ff76a668bb3bb8a54c5322e420", 3),
        ("20261014T092805Z-mute-events", "e3b2232c0c8770422c55d6500b692e54323a99e90e3a6a91385724d9310233c5", 26),
    ]:
        raw = (root / directory / "reports.jsonl").read_bytes()
        require(hashlib.sha256(raw).hexdigest() == expected_hash, f"Capture hash mismatch: {directory}")
        rows = [json.loads(line) for line in raw.splitlines()]
        require(rows[0]["usbId"] == "1395:018e" and rows[0]["interface"] == "03", "Wrong capture target")
        reports = [bytes.fromhex(row["hex"]) for row in rows if row["kind"] == "report"]
        require(len(reports) == expected_reports, "Unexpected report count")
        for report in reports:
            if report[:3] == bytes.fromhex("01ebfe"):
                require(len(report) == 107, "Truncated event")
                require(report[5:7] == b"\x01\x01", "Wrong event envelope")
                length = int.from_bytes(report[9:11], "little")
                require(4 <= length <= 94, "Invalid event body length")
                require(crc(report[13:13 + length]) == report[11], "Invalid event checksum")
                require(not any(report[13 + length:]), "Unexpected event padding")
                observed.add(report)
                count += 1
    fixtures = Path(__file__).resolve().parents[1] / "tests/MuteEventFixtures.h"
    prefixes = re.findall(r'"(01ebfe[0-9a-f]+)"', fixtures.read_text())
    require(len(prefixes) == 4, "Unexpected event fixture count")
    for prefix in prefixes:
        require(bytes.fromhex(prefix).ljust(107, b"\0") in observed, f"Fixture absent from capture: {prefix}")
    print(f"Both capture hashes verified; {count} vendor events pass CRC; all 4 event fixtures match raw Linux reports.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]))
