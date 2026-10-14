# Firmware handoff review — 2026-10-14

Current status: the later [successful Web capture](WEB_SUCCESS.md) resolves the
normal transfer path and supports the narrow native implementation described there.
The review below records what was known before that live capture.

Follow-up: [non-flashing Web/Windows protocol research](PROTOCOL_RESEARCH.md)
resolves ownership byte order and the Web retry predicate, and corroborates
the offset/validation-pending ambiguities. It does not validate installation.

Source: `epos-linux-firmware-handoff-20261014-115415.zip`, 14,108,009 bytes,
SHA-256 `5588524f5add865d3e584b91d43e93fbbd3d7396fa07ad2a85d0847556f60ad5`.
Extracted under the Git-ignored `captures/windows/epos-linux-firmware-handoff-20261014-115415/`.
The archive contains two sealed bundles, `epos-firmware-evidence-20261014-112243`
and `epos-firmware-evidence-20261014-115415` (called original and addendum below).
All **90 + 88 listed file hashes** verify, with full manifest coverage. Archive
paths were checked for traversal, duplicates and symlinks before extraction.

## Observed and documented evidence

- Original `captures/firmware-version.pcap`, frames **1/3**, at
  **11:37:04.585752/591738 UTC**: getter/reply identify **1.5.64**.
  The bundle's read-only validator checks raw HID/USB agreement, CRCs, frame
  references and timestamps. This agrees with the preceding live Linux read.
- One Windows UI check at **11:26:37.293952 UTC** leads to an empty incremental
  `GetUpdates` response, then a full `GetAllUpdatesAndConfigs` response offering
  **1.5.619** at **11:26:40.911 UTC**. Evidence: original `actions.csv`,
  `catalog/*-response.json`, `logs/agent-update-excerpts.txt` and screenshots.
- Catalog identity is `IMPACT_860T_ANC`, type Firmware, variant FW, default true.
  The observed headset is `1395:018e`, IMPACT 860T ANC. No comprehensive hardware
  revision eligibility or anti-rollback contract is established.
- Official URL: `https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_1.5.619.zip`.
  Windows downloaded it over credential-free HTTPS with normal certificate
  verification. ZIP SHA-256:
  `1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd`.
- ZIP has three members: `PackageInfo.xml`, `IMPACT860TANCReleaseNote.txt`, and
  `IMPACT_860T_ANC_v1.5.619.bin`. Every member matches the offer hash and the
  supplied inspection copy. Image: **6,774,556 bytes**, SHA-256
  `ab0b74926fef1b3bfbcdb3f92b0270ef7f730fe2bc0226914082c7286af43978`.
- Vendor release notes date 1.5.619 to **2026-07-31**: sporadic reboot improvement
  and removal of the “Power On” voice prompt on USB headsets. Catalog deployment,
  HTTP modification and ZIP timestamps are separate dates.

## Static evidence, not live validation

The local `dciadapter.dll` SHA-256
`93d4c97315d9a87acd7bb9748092b212222387a74536e437281736dbc31a98ad`
matches the addendum source. **10,317 instruction rows and five literal ranges**
were compared byte-for-byte against that DLL. This verifies the provenance of
the listing; it does not validate every inferred behavior.

- Application-update transport matches interface **04**, unlike settings on 03:
  output report 5 (255 bytes), input report 6 (66 bytes), feature report 3 (4 bytes).
  The interface has interrupt IN but no interrupt OUT. Output uses a separate
  opcode/length framing, not the settings CRC envelope.
- Nominal flow: feature connect, abort, sync, start, device-requested data slices,
  image validation, reboot/reconnect, postboot negotiation and commit.
  Fourteen supplied report layouts are **synthetic and never transmitted**.
- Compatibility serializer uses `[0x19,0x12,major,minor,patch,method]`; internal
  Application method 10 maps to wire compatibility method **4**. No compatibility
  query for this offer was observed. Ownership claim/release also remain untested.
- The image is a bounded sequence of eight-byte tags, BE32 lengths and payloads:
  `APPUHDR5`, seven `PARTDATA` records, then a 256-byte `APPUPFTR`.
  The footer's cryptographic meaning is unknown.

## Native implementation

`app/firmware/PackageInspection.*` provides bounded, in-memory ZIP inspection via
libarchive. It checks the archive hash before parsing; rejects unsupported entry
types, links, paths, duplicates, extra/missing files and oversized data; verifies
member hashes, exact manifest model/version/member agreement, container bounds,
partition uniqueness and footer placement. It never extracts files to disk.
The parser intentionally supports the observed package shape, not arbitrary
EPOS products or future container formats.

```sh
base=captures/windows/epos-linux-firmware-handoff-20261014-115415
original="$base/epos-firmware-evidence-20261014-112243"
./build/release/headset-controller \
  --inspect-firmware "$original/packages/IMPACT_860T_ANC_1.5.619.zip" \
  --firmware-offer "$original/catalog/headset-offer.json" \
  --installed-version 1.5.64
python3 scripts/verify-firmware-evidence.py "$base" /path/to/dciadapter.dll
```

The source DLL argument is optional. The verifier neither executes vendor code
nor contacts a device or network. Captures, DLLs and firmware are not build/test
dependencies or shipped assets. CTest's `firmware-package` suite constructs
synthetic ZIPs and covers valid inspection, version ordering, corruption,
manifest mismatch, traversal/links/duplicates, malformed records and size bounds.

The real package passed native inspection. Its report explicitly distinguishes
integrity relative to an editable supplied offer from publisher authenticity,
live catalog checking, device compatibility and installation. `--installed-version`
is a caller-provided comparison baseline, not a hardware read.

### Superseded GUI package workflow

The file-oriented GUI described below was an intermediate implementation. It has
been removed in favor of automatic live checks and version/update notifications;
see [current catalog evidence and workflow](CATALOG.md). Offline inspection remains
developer tooling, not the normal application workflow.

`FirmwareController` now presents saved-offer loading, local ZIP inspection and
official HTTPS download on Updates. File I/O, hashes and archive parsing run on
an owned worker separate from the settings worker. Cancellation suppresses late
completion; new jobs wait for the bounded worker operation to finish. Download
progress and failures stay in the page, with no automatic transfer retry.

`Firmware::PackageDownload` uses a dedicated Qt network manager, normal TLS
verification, manual cookie/authentication controls, no redirects, a 64 MiB
body limit and both inactivity and whole-operation timeouts. No download occurs
on startup, page navigation or offer import. Synthetic transport tests cover
HTTP/TLS errors, redirection, partial/oversized bodies, cancellation and timeout.

Package evidence is independent of a headset connection. Comparison always uses
the currently selected headset's identity, becoming unknown after disconnect.
The GUI does not freeze an old device version into the package report. Release
notes are displayed as plain text, including any markup-like content. The file
picker, scrolling and report display are covered by normal/high-DPI UI tests.
Offer/result state is session-only; downloaded bytes are not saved or installed.

The production Linux controller/download backend was exercised against the
official 1.5.619 URL on 2026-10-14, using a temporary harness with empty discovery
(no device operations). It fetched **6,777,742 bytes**, verified ZIP SHA-256
`1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd`, all three
member hashes and the manifest/container successfully. This confirms package
acquisition on Linux, not live catalog discovery or a firmware installation.

Validation after GUI integration: all **13 Release and 13 ASan/UBSan CTest suites**,
QML lint, clang-format 21, relocated-install checks and `git diff --check` passed.

## Remaining implementation blockers

1. **Resolved for public release discovery:** a separate anonymous EPOS Connect
   Web catalog now provides live Production releases; see [CATALOG.md](CATALOG.md).
   The Windows authenticated/incremental endpoint remains distinct. Empty
   incremental responses still cannot establish “up to date.”
2. The transferred vector has not been fully traced to the original BIN. Whole
   BIN versus transformed contents must be resolved before data transmission.
3. The disassembly adds the device's nonzero offset inside a slice loop; its
   interpretation for multi-report requests/resume remains unresolved.
4. Validation accepts pending opcode `17`, but an apparent state/control-flow
   inconsistency leaves its delay/retry semantics unresolved. Pending is not success.
5. Ownership byte order is now statically resolved in the
   [follow-up investigation](PROTOCOL_RESEARCH.md), but independent-client ID
   allocation, actual compatibility, activation identity/timings, transport replay
   guarantees, recovery and signature/trust rules remain incomplete. The Web
   retry predicate is specifically IoPipeStalled, not arbitrary failures.

See addendum `analysis/sequence-and-limitations.md` and
`analysis/compatibility-and-ownership.md` for exact instruction references.
Historical logs record an October 12 update failure at 0%, waiting for the first
data request; they do not demonstrate a successful transfer or its cause.
The handoff reserves the actual upgrade for Linux. No update authorization is
implied by importing or inspecting this package.
