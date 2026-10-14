# Successful Web upgrade and native implementation scope — 2026-10-14

The user performed **1.5.64 → 1.5.619** using EPOS Connect Web after correcting
the interface-04 permissions described in [WEB_ATTEMPT.md](WEB_ATTEMPT.md).
The assistant captured traffic and did not initiate the vendor upgrade.

## Preserved evidence

- Capture: `captures/linux/20261014T140256Z-web-firmware-upgrade/usb.pcap`
  (Git-ignored, with topology snapshots, descriptors, metadata and SHA256SUMS).
- Capture SHA-256: `15730f31f875ca801e0aac46642cbcfb00bdc0af0ca5ec65cde22d9848a5ee81`.
- Capture ready **14:03:00 UTC**, stopped **14:09:39 UTC**, exit status 0.
- **73,514 packets captured/received, zero kernel drops**, no snaplen truncation.
- Filter retained control/interrupt traffic for the headset and new addresses;
  bulk/ISO and initially unrelated device addresses were excluded.
- Firmware replies: frame **22** at **14:06:51.645541 UTC** = **1.5.64**;
  frame **73283** at **14:09:04.147357 UTC** = **1.5.619**. Both CRCs verified.
- Concatenating Data04 image payloads produces exactly **6,774,556 bytes**, SHA-256
  `ab0b74926fef1b3bfbcdb3f92b0270ef7f730fe2bc0226914082c7286af43978`, matching
  the complete official image, including all container records and footer.

## Wire contract established for this transition

1. Feature connect `03 01 02 00`; accept `06 01 00` plus zero padding.
2. Abort07/ack08, sync13 with `01020304`, reply14 payload
   `00 01 02 03 04 05`: initial resume point 0 and trailing byte 5.
3. Start01/reply02 payload `00 06 66`; start-data15.
4. **9,090** DataReq03 messages, each with BE32 byte count and **zero offset**.
   **27,229** Data04 messages, up to **249 image bytes** plus a final-image flag.
   Exactly one flag is 1, on the last image slice. Output reports are 255 bytes;
   report-6 input is **13 bytes**, matching this descriptor's 12-byte payload.
   The separate report-2 input has 66 wire bytes; it is not report 6.
5. Validate16/ack0b; 0c payload00 starts activation/reconnection. The hub and
   headset disappear and return; the trace alone does not establish why the
   upstream hub also re-enumerated. Headset address changes **28 → 30**.
6. Feature connect, sync13/reply14 `03 01 02 03 04 05` (resume point 3),
   start01/reply02, 0e00/ack0f, 1000/ack12, feature disconnect `03 01 07 00`.
7. A fresh Secom firmware read establishes **1.5.619**, also reported by the user.

All update output and feature submissions completed with USB status 0.
No nonzero offset, validation-pending, output retry or failure-recovery path was
exercised. These branches remain unsupported by the native session.

## Compatibility and ownership correction

Frame272 sends compatibility payload `19 00 12 00 01 00 05 00 6b 02 02 00`:
the observed wire method is **2**, not the offline candidate 4. Frame274 echoes
the payload with Secom header byte6 **0x10**, mapped to result **3001 / CommandUnknown**.
Frame276 sends claim payload `19 00 0f 00 00 00 60 02`; frame278 carries header
byte6 **0x20**, mapped to **3002 / CommandNotSupported**. Its body must not be
misinterpreted as a successful ownership acknowledgement.

The successful update therefore did not establish device-side ownership or a
positive device compatibility decision. Native preflight now builds its request
from the selected target version and accepts explicit compatibility or the
vendor's `CommandUnknown`/`CommandNotSupported` fallback. Incompatibility and
other device errors stop the operation. It also checks the fresh installed
version against the user's selected source version. It does not copy a vendor
client ID or claim authenticated/exclusive device ownership. Independent-client
ownership allocation and publisher signature verification remain unresolved.

## Native implementation and limits

- Independent domain state machine, Linux transport, asynchronous controller
  and QML presentation; no vendor runtime or architectural dependency.
- **1395:018e** uses the same application-update flow for future official production
  releases. The original hard-coded 1.5.64 → 1.5.619 allowlist was removed at the
  user's request. Version, package URL and ZIP hash come from the live catalog;
  manifest model/version and image container are verified before transfer.
- User confirmation is bound to the connection and target version. Settings
  operations are reserved during download/install. Download/package rejection
  precedes update-session commands; fresh firmware/preflight checks follow.
- Validated HID handles/descriptors and advisory locks on both interfaces;
  visible other-process HID handles cause refusal. Users close other controllers.
  This does not enforce exclusion against software ignoring locks or opening
  the device later, and unreadable `/proc` entries cannot be inspected.
- Same USB port, model, serial and a changed HID generation are required after
  reboot. The raw serial stays local; the journal filename uses its SHA-256.
- Sleep/shutdown inhibitor, UI close guard, bounded reads/reconnection/overall
  deadline, no automatic resends, rollback, offset guessing or resume.
- Phase records are atomically written and fsynced before mutations. An uncertain
  outcome or interrupted process blocks subsequent native transfer attempts for
  that serial, including across restarts/port changes. Use EPOS recovery/support;
  deleting the record is not a supported recovery procedure.
- Commit acknowledgement alone is insufficient: fresh firmware must match the
  target, and the successful journal update must persist.

**The native USB implementation has not flashed hardware.** The headset is now
current; reinstall/downgrade is rejected. Full-wire replay matches the successful
vendor trace, and simulated failures exercise stop/no-retry behavior. Neither
these tests nor one vendor success establish general recovery guarantees.

## Reproduce offline

```sh
python3 scripts/inspect-usb-firmware.py \
  captures/linux/20261014T140256Z-web-firmware-upgrade/usb.pcap \
  /tmp/opencode/epos-success-replay
HEADSET_UPDATE_REPLAY=/tmp/opencode/epos-success-replay \
  ./build/release/firmware-update-test optionalFullWireReplay
HEADSET_UPDATE_ARCHIVE=/path/to/IMPACT_860T_ANC_1.5.619.zip \
  ./build/release/firmware-update-test optionalOfficialPackage
```

The extractor rejects a different input hash or existing output directory.
Normal CTest uses a synthetic image with the captured request run lengths;
vendor images/captures are not committed. Full-wire replay compares every native
feature/output byte to the preserved capture and feeds its actual input reports.

Validation: all 14 Release and 14 ASan/UBSan CTest suites, QML lint,
clang-format 21 and relocated-install checks passed. Optional full-wire replay
and pinned official-package preparation also passed without USB access.
