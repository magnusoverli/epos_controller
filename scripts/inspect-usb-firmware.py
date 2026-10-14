#!/usr/bin/env python3
"""Offline extraction of the pinned successful EPOS Web USB capture. No USB access."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import struct

CAPTURE_HASH = "15730f31f875ca801e0aac46642cbcfb00bdc0af0ca5ec65cde22d9848a5ee81"
IMAGE_HASH = "ab0b74926fef1b3bfbcdb3f92b0270ef7f730fe2bc0226914082c7286af43978"


def extract(path):
    if not path.is_file() or path.stat().st_size > 256 * 1024 * 1024:
        raise ValueError("Expected a bounded regular capture file")
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != CAPTURE_HASH:
        raise ValueError("Capture does not match the preserved successful attempt")
    if struct.unpack_from("<IHHIIII", raw) != (0xa1b2c3d4, 2, 4, 0, 0, 245824, 220):
        raise ValueError("Unexpected PCAP header")
    position, frame = 24, 0
    reports, settings, requests = [], [], []
    pending = {}
    image = bytearray()
    data_flags = collections.Counter()
    while position < len(raw):
        sec, usec, captured, original = struct.unpack_from("<IIII", raw, position)
        position += 16
        packet = raw[position:position + captured]
        position += captured
        frame += 1
        if captured != original or len(packet) != captured or captured < 64:
            raise ValueError(f"Truncated frame {frame}")
        urb, event, kind, endpoint, device, bus, setup_flag, _, _, _, status, length, size = (
            struct.unpack_from("<QBBBBHccqiiII", packet)
        )
        if len(packet) != 64 + size or bus != 3:
            raise ValueError(f"Unexpected USB frame {frame}")
        data = packet[64:]
        row = dict(frame=frame, time=sec + usec / 1e6, device=device, event=chr(event),
                   endpoint=endpoint, status=status, length=length, data=data.hex())
        if event == ord("S"):
            pending[urb] = row
        elif event == ord("C") and urb in pending:
            submission = pending.pop(urb)
            submission.update(completionFrame=frame, completionStatus=status)
        setup = struct.unpack_from("<BBHHH", packet, 40) if kind == 2 and setup_flag == b"\0" else None
        if setup:
            row["setup"] = setup
        if device not in (28, 30):
            continue
        if setup and setup[:2] == (33, 9) and setup[3] == 4:
            reports.append(row)
        if kind == 1 and data and endpoint == 137:
            reports.append(row)
        if len(data) == 107 and data[:4] == bytes.fromhex("01ebfe01") and data[9] % 2 == 0:
            body = data[13:13 + data[9]]
            words = struct.unpack("<" + "H" * (len(body) // 2), body)
            if words and (words[0] == 25 or words[:2] == (1, 3)):
                row["words"] = words
                settings.append(row)
                crc = 255
                for byte in body:
                    crc ^= byte
                    for _ in range(8):
                        crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
                if crc != data[11]:
                    raise ValueError(f"Invalid Secom CRC in frame {frame}")
        if setup and setup[:4] == (33, 9, 0x205, 4):
            n = int.from_bytes(data[3:5], "big")
            if len(data) != 255 or data[0] != 5 or n > 250 or data[1] != n + 3 or any(data[5+n:]):
                raise ValueError(f"Invalid application output in frame {frame}")
            if data[2] == 4:
                image.extend(data[6:5+n])
                data_flags[data[5]] += 1
        if kind == 1 and endpoint == 137 and data and data[0] == 6 and data[2] == 3:
            requests.append(struct.unpack(">II", data[5:13]))
    if frame != 73514 or len(image) != 6774556 or hashlib.sha256(image).hexdigest() != IMAGE_HASH:
        raise ValueError("Capture/image counts or hash do not match")
    if len(requests) != 9090 or any(offset for _, offset in requests) or sum(n for n, _ in requests) != len(image):
        raise ValueError("Unexpected data request coverage")
    if data_flags != {0: 27228, 1: 1}:
        raise ValueError("Unexpected end-of-image flags")
    if any(row.get("completionStatus") != 0 for row in reports if row["event"] == "S"):
        raise ValueError("Unacknowledged update output")
    versions = [row["words"][2:] for row in settings if row["event"] == "C" and row["words"][:2] == (1, 3)]
    if versions != [(1, 5, 64), (1, 5, 619)]:
        raise ValueError("Unexpected firmware readback")
    return dict(reports=reports, settings=settings), bytes(image)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path, help="New directory; existing paths are never overwritten")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output directory already exists")
    try:
        decoded, image = extract(args.capture)
    except (OSError, ValueError, struct.error) as error:
        parser.error(str(error))
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "decoded-update.json").write_text(json.dumps(decoded, indent=2) + "\n")
    (args.output / "transferred-image.bin").write_bytes(image)
    (args.output / "SHA256SUMS").write_text("".join(
        f"{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.name}\n"
        for file in sorted(args.output.iterdir())
    ))
    print("Verified 73,514 frames, 9,090 requests, 27,229 data reports and installed version 1.5.619.")
    print(f"Transferred image SHA-256: {IMAGE_HASH}")


if __name__ == "__main__":
    main()
