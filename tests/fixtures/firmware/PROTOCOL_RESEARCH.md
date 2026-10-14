# Non-flashing protocol investigation — 2026-10-14

## Outcome

**Superseding live evidence:** [successful Web upgrade](WEB_SUCCESS.md) establishes
the exact normal transfer/commit sequence and corrects the input-report size and
observed compatibility method. The sections below retain their original static
findings; their no-live-capture conclusions are historical.

Subsequent live evidence: [Web attempt blocked before transfer](WEB_ATTEMPT.md)
identified root-only access to interface 04 and a misleading vendor error.

Windows is **not required for further protocol research**. The current EPOS Web
updater contains an independently inspectable implementation of the same
Application update protocol as the Windows adapter. This investigation ran on
Linux and sent **no headset commands**, including no compatibility, claim,
connect, abort, start, data, reboot or commit requests. Vendor code was inspected
as data, never executed.

New results: ownership serialization and status mappings, the exact retry
predicate, an independent HID usage selector, and corroboration of the transfer,
validation and reconnect/commit flow. Two problematic branches are present in
both implementations; they cannot be dismissed as an x86 disassembly artifact.
These findings improve the protocol specification, but do **not** establish a
safe, validated installer or successful recovery behavior.

## Sources and reproduction

- Web WASM: `https://connect.eposaudio.com/dci/dci-wasm.wasm`, 2,265,138 bytes,
  SHA-256 `4d91c621fff3402874639d1978a75c90fada104f27eeb67d0555be98560a13f8`.
- Web glue: `https://connect.eposaudio.com/dci/dci-wasm.js`, SHA-256
  `e73cec982a9372e462e8335a4a317226f15637bae90880c5791ddcfd7d81db8c`.
- Windows `dciadapter.dll`: SHA-256
  `93d4c97315d9a87acd7bb9748092b212222387a74536e437281736dbc31a98ad`.
- Original package/captures: [HANDOFF.md](HANDOFF.md); catalog: [CATALOG.md](CATALOG.md).

WABT 1.0.36 was extracted into a temporary directory without system installation.
The module has 72 imported functions, 10,595 defined functions and five passive
data segments. Function 76 initializes their memory addresses.

Reproduce original WAT functions, binary offsets/hashes and literal/vtable evidence:

```sh
python3 scripts/inspect-web-firmware.py /path/to/dci-wasm.wasm \
  /path/to/new-evidence-directory --wasm2wat /path/to/wasm2wat
```

The extractor rejects a different module hash and existing output directory.
It does not fetch, instantiate or execute the vendor module. Local output is in
`captures/linux/20261014-web-firmware-research/` (Git-ignored). Function hashes
cover the locals declaration and instruction body, not the size prefix.
Function numbers and memory addresses below apply **only** to this pinned module.

Verification: all 25 extracted function-body hashes and the local evidence
manifest passed, as did rejection of a modified WASM and an existing output
directory. A further **343 Windows disassembly rows** from the stream
constructors/uint32 writer were independently matched against the pinned DLL's
PE sections. These checks verify provenance and extraction, not device behavior.

Exploratory C-like decompilation was only a navigation aid: WABT's decompiler
omits substantial exception-region bodies. An exploratory normal-path-only
transformation is not equivalent to the original program and is not evidence
for exception cleanup/recovery. Rotations also need checking in WAT. Findings
below were checked against original WAT and, where noted, Windows instructions.

## 1. Ownership byte order resolved statically

The generated Secom vtable at `0x57868` resolves claim/release/compatibility
slots to functions **6005**, **6006**, **6008**. Claim/release construct streams
with 16-bit mode 1 and 32-bit mode 2 (**5983**, **6157**). Functions **6161**,
**5992** and **5993** serialize/deserialize those values.

| Operation | Payload before the existing Secom envelope/CRC |
|---|---|
| Claim | `LE16(0x19), LE16(0x0f), mixed32(client_id)` |
| Release | `LE16(0x19), LE16(0x10), mixed32(client_id)` |
| Compatibility | `LE16(0x19), LE16(0x12), LE16(major), LE16(minor), LE16(patch), LE16(method)` |

`mixed32` means **high 16-bit word first, little-endian bytes within each word**.
Byte shifts: **16, 24, 0, 8**, not ordinary little-endian uint32. Synthetic
illustration: `0x12345678` becomes `34 12 78 56`. Never sent to hardware; this
does not select an ownership ID for Linux.

Independent Windows confirmation: claim serializer `0x10322b80` passes the same
modes through constructor `0x1034d030` to `0x1034cdf0`, which selects shifts
16/24/0/8 at `0x1034ce93–0x1034cea8`. Uint32 writer `0x1034d8e0` uses those fields.

After the four-byte command header, claim reads uint16 status and mixed32 ID;
release reads uint16 status. Host wrapper mappings:

| Device status | Claim, function 5864 | Release, function 5867 |
|---|---:|---:|
| 0 | success (0) | success (0) |
| 1 | result 1110 | result 10000 |
| 2 | result 1120 | result 1120 |
| other | result 3011 | result 3011 |

These are static mappings, not observed replies. Claim does not independently
validate ownership by checking the returned ID. Ownership changes session state.

### IDs differ between implementations

Function **7469**'s table at `0x66784` contains
`0x160, 0x260, 0x360, 0x60, …, 0x460`; Application/internal method 10 contributes
**0x460** in this Web binary, versus **0x430** in Windows. Web additionally ORs
`0x10000` for transport enum 2 and `0x20000` for enums 3/4.

The Windows-derived `0x430` is therefore **not a universal Application protocol
ID**. Allocation/meaning of the low bits, an appropriate independent-client ID
and this headset's ownership policy remain unestablished. Knowing the encoding
does not justify copying either vendor ID into the Linux application.

## 2. Compatibility corroborated; actual decision unknown

Function **6008** constructs the same six uint16 values as Windows; **7469** maps
internal Application method 10 to wire method **4**. The candidate 1.5.619 payload:

```
19 00 12 00 01 00 05 00 6b 02 04 00
```

This is an **offline serialization example**, not a recorded transaction or a
complete HID report. Reply payload after the header has uint16 status and
alternative method. **5883** handles statuses 0/1/2 and rejects unknown status
with result 3011. Outer logic rejects incompatibility and checks alternatives.
The vendor's unsupported-command fallback does not prove that bypassing this
check is appropriate. No device-side answer or anti-rollback policy was obtained.

## 3. HID interface and framing independently corroborated

Function **5652** passes memory `0x713f0` to the HID-interface selector. Its
literal is `00 ff 01 00`: usage page **0xff00**, usage **1**, matching Windows
and the observed normal-mode interface **04** descriptor.

Function **7502** formats the output buffer:

```
report 5 | payload_length + 3 | opcode | BE16(payload_length) | payload | padding
```

Buffer size comes from the interface. The observed interface uses 255-byte
output reports, 66-byte input reports and four-byte feature reports. Data `04`
has a one-byte EOF flag, leaving **249 image bytes** per full output report.
Interface 04 has no interrupt OUT, so this does not imply interrupt-OUT writes.

## 4. Retry policy narrowed

Function **7502** installs an equality predicate via vtable `0x665d8`, slot 6 →
**7509**, comparing with **4010**. Function **2374** identifies that result as
**IoPipeStalled** (name pointer `245786`).

Budget: **three retries / four total attempts**, 500 ms between retries, with
a 30,000 ms low-level output-operation timeout. It does not retry every error,
timeout or removal. This does not establish partially delivered report replay
safety or which Linux hidraw errors correspond to this vendor result.

## 5. Nonzero offsets remain ambiguous

Transfer function **7471** decodes DataReq `03` as two BE32 values: requested
byte count and offset. In original WAT, **inside** the packet-slicing loop,
local 17 (offset) is added to local 6 (running cursor), followed by a bounds
check, copy and cursor advancement by bytes sent. The back-edge does not zero
the offset. This agrees with Windows `0x1035c0d4` and its back-edge.

Neither “absolute offset” nor “relative skip applied once per DataReq” can be
assumed from this code. A nonzero offset spanning multiple output reports may
be added repeatedly. No actual nonzero request or resume exchange from this
headset is captured. Device/protocol evidence is still needed here.

## 6. Validation-pending inconsistency corroborated

Function **7518** sends query `16`, accepts `0b`/`17` and reads one delay byte
on the `17` path. The ordinary accepted-response path writes **internal state
9 for either opcode**. The later `17` handler checks whether state is already
9 and skips storing the delay/entering state 8 if so. Original WAT contains
both the state-9 store and this guard.

Function **7471** polls only while state is 8. This corroborates Windows
`0x1035d156`, rather than resolving the inconsistency. It does not prove a
device bug or that this headset ever sends `17`. **A pending opcode must not
be treated as successful validation** based on this internal state alone.

The outer validation timer is nominally 90 seconds, but individual receives
have a 100-second deadline. The outer check is not a hard 90-second wall-clock
limit. Actual validation timing remains unmeasured.

## 7. Activation includes a postboot transaction

Functions **7512**, **7471**, and callback **7407** corroborate the nominal flow:

1. Feature-connect; initial Abort `07`/`08` only with local resume point zero.
2. Sync `13`/`14`, verify file ID, Start `01`/`02`; start-data `15`.
3. Device DataReq `03` / host Data `04` transfers.
4. Validation `16`; transfer-complete indication `0b`; response `0c 00`.
5. Reconnect callback receives 90,000 ms, waits through device lifecycle
   handling, replaces the HID I/O handle using the same usage selector.
6. Negotiate again without the initial abort; require postboot resume point
   **3**; `0e 00` → `0f`, then `10 00` → `12`.
7. Record resume point 4 and clean up. Final receive waits are 30,000 ms each.

This does not establish bootloader VID/PID, enumeration identity matching,
power-loss recovery or host disappearance between activation and commit.
Reconnect alone is not independent verification of the active firmware version;
our eventual installer must read identity after completion.

## 8. Image-provider tracing remains incomplete

XML parser **10204** recognizes `SeComAppHSFWU`, `FWFiles`, `HeadsetFWFile` and
`FWVersion`/`HeadsetVersion`. The transfer function obtains a shared byte vector
through FirmwareBundle and copies from its start plus cursor. It strips no
partition headers or footer in that transfer loop.

`CFirmwareBundle` vtable `0x73354`, slot 2 → **10009**, returns a cached vector
or invokes a separate firmware-blob provider. ZIP extraction/package selection
were traced separately, but the complete selected-package → cached/provider
vector chain is **not independently closed** for this image. Sending the whole
original BIN remains strongly supported, not live validated. Signature algorithm,
trust anchor, device authentication and anti-rollback guarantees remain unknown.

## What can finish without upgrading this headset?

- Further offline tracing, an independent codec/state machine and mock-device
  tests of framing, bounds, stale replies and failures can all run on Linux.
- A coordinated vendor **check-only** capture may corroborate capability and
  compatibility reads, if that workflow sends them. It cannot establish actual
  transfer, validation, reboot and commit behavior.
- The strongest remaining evidence is a complete successful vendor-updater USB
  capture on a **spare equivalent headset**, including identity before/after
  and postboot commit. Windows is one option; the Web updater is another where
  the model/browser is supported. Either actually upgrades that test device.
  No such operation is authorized or performed here.
- Resolve the outstanding branches/recovery strategy before this user's Linux
  upgrade. Starting and cancelling at 0% is not a non-mutating experiment:
  ownership, abort and start change update state before progress is visible.
