# Firmware identification — 2026-10-14

Firmware version is a read-only identity value, not a writable device setting.
The GUI reads it automatically using the same serialized HID worker/connection
checks as settings. Home, Device settings and Updates show the same live version.
Reconnection invalidates the old value and starts a fresh read; automation modes
remain descriptor-only. `--read-firmware [--device USB-key]` provides explicit
headless JSON output, with exit code 4 for failure.

## Evidence

- Windows enumeration capture `00-enumeration-reviewed.pcapng`, frames **135/137**:
  `[01,03] -> [01,03,1,5,64]`, five little-endian 16-bit words in the reply.
- `dciadapter.dll` (hash/image base in ADAPTER.md), generated vtable `104ba7d8`,
  slot `264`: `QtilSecomparserGenerated::Service_GetSwVersion`, function `1031e980`.
  It constructs `[01,03]` and expects a ten-byte reply body.
- `tests/FirmwareFixtures.h` stores independent captured prefixes. The capture
  verifier compares the padded reports directly to the raw frame references.
- Live Linux command `./build/release/headset-controller --read-firmware`
  returned **1.5.64** on USB key `3-1.1` on 2026-10-14. This agrees with Windows.
  `bcdDevice=0105` remains a USB descriptor revision, not a substitute firmware value.

The decoder requires a matching reply operation and exactly three version words.
Versions are compared numerically as a triple, not lexically. No version is
invented from an unavailable/failed read. Firmware refresh does not change
settings, and neither metadata refresh nor reconnection replays a write/reset.

## Newer releases and installation

Native live release checking is implemented; firmware installation is **not implemented**.
See the [current public catalog evidence](../firmware/CATALOG.md).
Offline package inspection is implemented; see the [October 14 handoff review](../firmware/HANDOFF.md).
Do not interpret the installed version as confirmation that the headset is current,
or a saved offer as the current live catalog.

The received Windows evidence establishes a fresh offer for **1.5.619** on
2026-10-14 at 11:26:40 UTC and includes its original ZIP, manifest, release notes
and matching SHA-256 values. The full `GetAllUpdatesAndConfigs` response offers it
even though incremental `GetUpdates` returns `Updates:[]`. Both requests used an
existing Token/tenant registration; its bootstrap contract remains unknown.
Credential-free package download succeeded on Windows. `CloudWhiteList.txt` at
the storage host is not this model's release catalog.

EPOS's support article lists its desktop and Web update workflows:
https://www.eposaudio.com/en-us/support/knowledge-base-enterprise/software/epos-connect/epos-connect---updating-your-epos-products-firmware/

The old Web host redirects asset requests to the new landing page and discards
their paths. Following the new host `https://connect.eposaudio.com/` directly
resolved this research blocker. Its public assets establish the anonymous catalog
endpoint now used by the app. No vendor JavaScript/WASM was executed.

Remaining inputs:

1. Establish device-specific compatibility restrictions and downgrade constraints.
   Public release discovery no longer requires the Windows Token/bootstrap flow.
2. Resolve the transferred image-provider bytes, nonzero offset semantics,
   validation-pending replies and ownership guard encoding from static evidence
   or an explicitly authorized update trace. The user reserved the upgrade for Linux.
3. Establish compatibility, activation/re-enumeration, recovery and any
   signature/trust verification. A 256-byte footer alone does not establish a signature.

Implement catalog/package/transfer components in the independent firmware domain.
Do not turn the scalar settings session into an unverified firmware flasher.
