"""Offline cross-check of packet fixtures/checksum against the reviewed Windows bundle.

No hardware I/O or dependencies outside Python's standard library. Bundle originals
remain untouched. Usage: python3 scripts/verify-settings-evidence.py <reviewed-run>
"""
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def packets(path):
    data = path.read_bytes()
    offset, endian, interfaces = 0, "<", []
    while offset < len(data):
        require(offset + 12 <= len(data), "Truncated PCAPNG block")
        if data[offset:offset + 4] == bytes.fromhex("0a0d0d0a"):
            endian = {bytes.fromhex("4d3c2b1a"): "<", bytes.fromhex("1a2b3c4d"): ">"}[data[offset + 8:offset + 12]]
            interfaces = []
        kind, size = struct.unpack_from(endian + "II", data, offset)
        require(size >= 12 and size % 4 == 0 and offset + size <= len(data), "Invalid block length")
        require(struct.unpack_from(endian + "I", data, offset + size - 4)[0] == size, "Block trailer mismatch")
        if kind == 1:
            interfaces.append(struct.unpack_from(endian + "H", data, offset + 8)[0])
        elif kind == 6:
            interface, _, _, captured, original = struct.unpack_from(endian + "IIIII", data, offset + 8)
            require(captured == original and 32 + ((captured + 3) // 4) * 4 <= size, "Truncated packet")
            require(interfaces[interface] == 249, "Expected USBPcap link type")
            yield data[offset + 28:offset + 28 + captured]
        offset += size


def crc8(body):
    crc = 255
    for byte in body:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
    return crc


def main(root):
    for line in (root / "SHA256SUMS.txt").read_text(encoding="utf-8-sig").splitlines():
        digest, name = line.split(None, 1)
        path = root / name.strip().removeprefix("*")
        require(path.resolve().is_relative_to(root.resolve()), "Checksum path outside bundle")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == digest, f"Hash mismatch: {name}")
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8-sig"))
    require((manifest["vid"], manifest["pid"]) == ("1395", "018e"), "Wrong headset identity")
    observed, count, raw_frames = set(), 0, {}
    for capture in manifest["captures"]:
        path = root / capture["file"]
        require(path.resolve().is_relative_to(root.resolve()), "Capture path outside bundle")
        require(hashlib.sha256(path.read_bytes()).hexdigest() == capture["sha256"], "Capture hash mismatch")
        frames = list(packets(path))
        require(len(frames) == capture["packet_count"], "Capture packet count mismatch")
        for index, frame in enumerate(frames, 1):
            require(len(frame) >= 27, "Truncated USBPcap header")
            header_size = int.from_bytes(frame[:2], "little")
            payload_size = int.from_bytes(frame[23:27], "little")
            require(27 <= header_size <= len(frame) and header_size + payload_size == len(frame), "USBPcap data length mismatch")
            report = frame[header_size:]
            if report.startswith(bytes.fromhex("2109010203006b00")):
                report = report[8:]
            raw_frames[(capture["file"], index)] = report
            if len(report) != 107 or report[:3] != bytes.fromhex("01ebfe"):
                continue
            length = int.from_bytes(report[9:11], "little")
            require(length <= 94, "Vendor body exceeds report")
            require(report[11:13] == bytes([crc8(report[13:13 + length]), 0]), f"CRC mismatch: {path.name}:{index}")
            count += 1
            observed.add(report)
    transactions = [json.loads(line) for line in (root / "analysis/transactions.jsonl").read_text().splitlines()]
    for transaction in transactions:
        for key, frames in [("full_request_hex", "request_frames"), ("full_response_hex", "response_frames")]:
            frame = transaction[frames]
            require(len(frame) == 1, "Fixture expects a single report frame")
            require(raw_frames[(transaction["capture_file"], frame[0])] == bytes.fromhex(transaction[key]), "Transaction does not match its raw capture frame")
    fixture = Path(__file__).resolve().parents[1] / "tests/SettingsCaptureFixtures.h"
    prefixes = re.findall(r'"(01ebfe[0-9a-f]+)"', fixture.read_text())
    require(len(prefixes) == 34, "Unexpected fixture count")
    for prefix in prefixes:
        report = bytes.fromhex(prefix).ljust(107, b"\0")
        require(report in observed, f"Test fixture missing from captures: {prefix}")
    enumeration = fixture.with_name("SettingsEnumerationFixtures.h").read_text()
    pairs = re.findall(r'// Enumeration frames (\d+)/(\d+)\..*?"(01ebfe[0-9a-f]+)".*?"(01ebfe[0-9a-f]+)"',
                       enumeration, flags=re.DOTALL)
    require(len(pairs) == 15, "Unexpected enumeration fixture count")
    capture_name = next(name for name, _ in raw_frames
                        if Path(name).name == "00-enumeration-reviewed.pcapng")
    for request_frame, reply_frame, request, reply in pairs:
        for frame, prefix in [(request_frame, request), (reply_frame, reply)]:
            require(raw_frames[(capture_name, int(frame))] == bytes.fromhex(prefix).ljust(107, b"\0"),
                    f"Enumeration fixture differs from raw frame {frame}")
    print(f"Hashes verified; {count} vendor reports ({len(observed)} distinct) match body CRC-8/0x07/init-ff.")
    firmware = re.findall(r'"(01ebfe[0-9a-f]+)"', fixture.with_name("FirmwareFixtures.h").read_text())
    require(len(firmware) == 2, "Expected one captured firmware request/reply pair")
    for frame, prefix in zip([135, 137], firmware):
        require(raw_frames[(capture_name, frame)] == bytes.fromhex(prefix).ljust(107, b"\0"),
                f"Firmware fixture differs from raw frame {frame}")
    print("Firmware version fixture matches enumeration frames 135/137.")
    print(f"All {len(transactions)} transaction request/reply pairs match cited raw frames; all 32 captured fixture pairs match captures.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]))
