#!/usr/bin/env python3
"""Extract static evidence from the pinned October 14 EPOS Web updater.

Usage: inspect-web-firmware.py DCI_WASM OUTPUT_DIRECTORY [--wasm2wat PATH]
Requires WABT's wasm2wat. No network/device access or vendor-code execution.
Outputs original WAT functions and binary offsets/hashes, not a runnable updater.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


EXPECTED = "4d91c621fff3402874639d1978a75c90fada104f27eeb67d0555be98560a13f8"
FUNCTIONS = {
    76: "passive data initialization",
    2374: "result code names",
    5652: "application HID usage selector",
    5864: "ownership claim status wrapper",
    5867: "ownership release status wrapper",
    5883: "compatibility status wrapper",
    5983: "output stream constructor",
    5992: "32-bit stream serializer",
    5993: "32-bit stream deserializer",
    6005: "ownership claim serializer",
    6006: "ownership release serializer",
    6008: "compatibility serializer",
    6157: "stream byte-order initialization",
    6161: "16-bit stream serializer",
    7407: "application reconnect callback",
    7469: "update selection compatibility and ownership",
    7471: "application transfer validation and commit",
    7486: "application connect",
    7502: "output framing and bounded retry",
    7509: "retry predicate",
    7512: "initial abort sync start negotiation",
    7518: "image validation response handling",
    7519: "application cleanup guard",
    10009: "firmware bundle byte-vector provider",
    10204: "package XML parser",
}
BASES = (131072, 131408, 494784, 495344, 500357)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inspect(source, output, wasm2wat):
    data = source.read_bytes()
    require(sha(data) == EXPECTED, "Wrong updater revision; do not reuse these function IDs on another binary")
    require(not output.exists(), "Output directory already exists; use a new directory")

    def leb(pos):
        value = 0
        for shift in range(0, 35, 7):
            byte = data[pos]
            pos += 1
            value |= (byte & 127) << shift
            if not byte & 128:
                return value, pos
        raise ValueError("Invalid LEB128")

    sections = {}
    pos = 8
    while pos < len(data):
        kind = data[pos]
        length, start = leb(pos + 1)
        require(start + length <= len(data), "Truncated section")
        sections[kind] = (start, length)
        pos = start + length

    count, pos = leb(sections[11][0])
    require(count == len(BASES), "Unexpected data segments")
    memory = bytearray(503040)
    segments = []
    for base in BASES:
        flags, pos = leb(pos)
        require(flags == 1, "Expected passive data")
        size, pos = leb(pos)
        require(base + size <= len(memory), "Unexpected data extent")
        memory[base:base + size] = data[pos:pos + size]
        segments.append({"memoryAddress": base, "fileOffset": pos, "size": size})
        pos += size

    count, pos = leb(sections[9][0])
    require(count == 1, "Unexpected element segments")
    flags, pos = leb(pos)
    require(flags == 0 and data[pos] == 0x41, "Expected table initialization")
    first, pos = leb(pos + 1)
    require(data[pos] == 0x0b, "Expected constant table offset")
    count, pos = leb(pos + 1)
    table = {}
    for index in range(first, first + count):
        table[index], pos = leb(pos)

    count, pos = leb(sections[10][0])
    require(count == 10595, "Unexpected function count")
    bodies = {}
    # The pinned module has 72 imported functions (plus one imported memory).
    for index in range(72, 72 + count):
        size, pos = leb(pos)
        bodies[index] = {"fileOffset": pos, "size": size, "sha256": sha(data[pos:pos + size])}
        pos += size
    require(pos == sum(sections[10]), "Code section length mismatch")

    wat = subprocess.check_output([wasm2wat, "--enable-all", str(source)], text=True, timeout=60)
    starts = list(re.finditer(r"^  \(func \(;(\d+);\)", wat, re.M))
    functions = {int(match[1]): wat[match.start():starts[i + 1].start()]
                 for i, match in enumerate(starts[:-1])}
    require(len(starts) == count, "Unexpected WAT function numbering")
    require(all(index in functions for index in FUNCTIONS), "Missing evidence function")

    # Check the initialization constants before using the reconstructed passive-data addresses.
    for segment in segments:
        pattern = (rf"i32.const {segment['memoryAddress']}\s+(?:global.set 1\s+)?"
                   rf"i32.const 0\s+i32.const {segment['size']}\s+memory.init")
        require(re.search(pattern, functions[76]), "Data initialization does not match the pinned layout")

    def words(address, count):
        return list(struct.unpack_from(f"<{count}I", memory, address))

    evidence = {
        "classification": "STATIC ONLY; no vendor execution, device operations or live update validation",
        "sourceUrl": "https://connect.eposaudio.com/dci/dci-wasm.wasm",
        "sha256": EXPECTED,
        "bytes": len(data),
        "dataSegments": segments,
        "functions": {str(index): {"description": description, **bodies[index]}
                      for index, description in FUNCTIONS.items()},
        "literals": {
            "applicationUsageAt0x713f0": words(0x713f0, 1),
            "methodOwnershipIdsAt0x66784": words(0x66784, 10),
            "uint32SecondByteShiftsAt0x57c20": words(0x57c20, 3),
            "releaseStatusResultsAt0x576fc": words(0x576fc, 3),
            "generatedSecomVtableClaimReleaseCompatibility":
                [table[words(address, 1)[0]] for address in (0x578c4, 0x578c8, 0x578d0)],
            "retryPredicateVtableSlot6": table[words(0x665f0, 1)[0]],
        },
    }
    output.mkdir(parents=True)
    for index in FUNCTIONS:
        (output / f"function-{index}.wat").write_text(functions[index])
    (output / "fingerprint.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"Verified pinned WASM; extracted {len(FUNCTIONS)} original function listings and literal evidence to {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--wasm2wat", default="wasm2wat")
    args = parser.parse_args()
    inspect(args.source, args.output, args.wasm2wat)
