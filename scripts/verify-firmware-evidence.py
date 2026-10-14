#!/usr/bin/env python3
"""Verify the extracted 2026-10-14 firmware handoff, optionally against the source DLL.

Usage: verify-firmware-evidence.py HANDOFF_DIRECTORY [DCIADAPTER_DLL]
No device commands, network requests, vendor-code execution, or bundle modifications.
"""
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import sys
import zipfile


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def main(root, adapter=None):
    first = root / "epos-firmware-evidence-20261014-112243"
    second = root / "epos-firmware-evidence-20261014-115415"
    count = 0
    for bundle, expected in [(first, 90), (second, 88)]:
        listed = set()
        for line in (bundle / "SHA256SUMS").read_text(encoding="utf-8-sig").splitlines():
            checksum, name = line.split("  ", 1)
            path = PurePosixPath(name)
            require(not path.is_absolute() and ".." not in path.parts and "\\" not in name, "Unsafe manifest path")
            require(name not in listed and re.fullmatch(r"[0-9a-f]{64}", checksum), "Invalid manifest entry")
            target = bundle / name
            require(not target.is_symlink() and digest(target.read_bytes()) == checksum, f"Hash mismatch: {name}")
            listed.add(name)
        actual = {p.relative_to(bundle).as_posix() for p in bundle.rglob("*")
                  if p.is_file() and p.name != "SHA256SUMS"}
        require(listed == actual and len(listed) == expected, "Incomplete manifest coverage")
        count += len(listed)
    offer = load(first / "catalog/headset-offer.json")
    variant = offer["UpdatesURLs"][0]
    package = first / "packages/IMPACT_860T_ANC_1.5.619.zip"
    require(digest(package.read_bytes()) == variant["Hash"], "Package hash differs from offer")
    with zipfile.ZipFile(package) as archive:
        require(len(archive.infolist()) == 3 and archive.testzip() is None, "Invalid package ZIP")
        for member in variant["Files"]:
            data = archive.read(member["Name"])
            require(digest(data) == member["Hash"], "Package member hash mismatch")
            require(data == (first / "packages/inspection" / member["Name"]).read_bytes(), "Inspection copy differs")
    full = load(first / "catalog/GetAllUpdatesAndConfigs-response.json")["response"]["ResponseBody"]["Updates"]
    require(next(item for item in full if item["IProduct"] == "IMPACT_860T_ANC") == offer, "Full response disagrees with offer")
    require(load(first / "catalog/GetUpdates-response.json")["response"]["ResponseBody"]["Updates"] == [], "Incremental-response evidence changed")
    examples = load(second / "analysis/synthetic-report-examples.json")["examples"]
    require(len(examples) == 14, "Expected 14 synthetic transfer examples")
    for example in examples:
        data = bytes.fromhex(example["reportHex"])
        require("NEVER TRANSMITTED" in example["classification"], "Example provenance changed")
        if example["kind"] == "feature":
            require(data in [bytes.fromhex("03010200"), bytes.fromhex("03010700")], "Unexpected feature example")
        else:
            length = int.from_bytes(data[3:5], "big")
            require(len(data) == 255 and data[0] == 5 and length <= 250 and data[1] == length + 3, "Invalid output framing")
            require(data[2] == example["opcode"] and data[5:5+length].hex() == example["payloadHex"], "Invalid example payload")
            require(not any(data[5+length:]), "Nonzero example padding")
    print(f"Verified {count} file hashes, original ZIP/member integrity, catalog-offer agreement and 14 synthetic layouts.")
    if adapter:
        data = adapter.read_bytes()
        require(digest(data) == "93d4c97315d9a87acd7bb9748092b212222387a74536e437281736dbc31a98ad", "Wrong adapter binary")
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        optional = pe + 24
        base = struct.unpack_from("<I", data, optional + 28)[0]
        section_count = struct.unpack_from("<H", data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", data, pe + 20)[0]
        sections = [struct.unpack_from("<IIII", data, optional + optional_size + i * 40 + 8)
                    for i in range(section_count)]

        def bytes_at(va, length):
            rva = va - base
            for _, start, size, offset in sections:
                if start <= rva and rva + length <= start + size:
                    return data[offset+rva-start:offset+rva-start+length]
            raise ValueError("Address outside backed PE sections")

        rows = 0
        for path in (second / "analysis").glob("*.txt"):
            for address, encoded in re.findall(r"^([0-9a-f]{8}) ([0-9a-f]+)\s", path.read_text(), re.M):
                raw = bytes.fromhex(encoded)
                require(bytes_at(int(address, 16), len(raw)) == raw, f"Disassembly differs: {path.name} {address}")
                rows += 1
        literals = load(second / "metadata/literal-evidence.json")["literals"]
        for literal in literals:
            raw = bytes.fromhex(literal["bytes"])
            require(bytes_at(int(literal["va"], 16), len(raw)) == raw, "Literal differs from binary")
        print(f"Verified {rows} disassembly instruction rows and {len(literals)} literal ranges against the matching DLL.")
    else:
        print("Source DLL not supplied; disassembly bytes were not independently compared.")


if __name__ == "__main__":
    if len(sys.argv) not in [2, 3]:
        raise SystemExit(__doc__)
    main(Path(sys.argv[1]), Path(sys.argv[2]) if len(sys.argv) == 3 else None)
