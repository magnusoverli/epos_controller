# Web update blocked before transfer — 2026-10-14

The user attempted EPOS Connect Web's update and reported a “multiple devices
connected” error. The Linux USB capture ran from 13:51:18 to 13:57:46 UTC:

- `captures/linux/20261014T135114Z-web-firmware-upgrade/usb.pcap` (Git-ignored).
- SHA-256 `7181df5fce0c3fef431059faf8b1423c3491fbbbc5869ddbcf0a312d57f8ebcf`.
- 955 packets captured, 955 received by the filter, zero kernel drops; no
  snaplen-truncated packets. USB_LINUX_MMAPPED, full control/interrupt reports.
- ISO/bulk and the other initially attached bus-3 device addresses were excluded;
  current and newly enumerated addresses and address-zero enumeration were retained.

## Observed

There are no Application update interface-04 SET_REPORTs or report-6 input
payloads in the capture. No application connect/abort/start/data/commit traffic
was observed, and no Secom compatibility/ownership requests were observed.
134 SET_REPORT submissions carry settings-channel report 1 on interface 03.
Firmware getter replies in frames **51** (13:53:28.614509 UTC, address 26) and
**608** (13:53:51.455522 UTC, address 28) both contain **1.5.64**, with verified
CRC-8. This was not a successful transfer capture.

The upstream hub `3-1` and headset `3-1.1` disappeared around 13:53:41 and
returned as addresses 27/28 by 13:53:48. Cause is not established by these USB
records; do not describe this as firmware activation or a proven hub failure.
The kernel retrieved the interface-04 HID descriptor in frame 510.

## Why the browser message may be misleading

In the pinned Web WASM described in [PROTOCOL_RESEARCH.md](PROTOCOL_RESEARCH.md),
function **7415**, called during update-method detection, searches enumerated
interfaces for usage **ff00:0001**. When that interface is absent, it logs:

> Device supports Application update but the corresponding interface was not enumerated

and throws result **1156** with text:

> Multiple devices connected, cannot determine which interface to use for the update (Emscripten limitation)

The Web UI maps `MultipleDevicesConnected` to the user-facing disconnect-other-
devices message. This branch does not establish that another physical headset
is attached. It precedes the firmware transfer sequence.

## Confirmed Linux access defect and correction

Before correction, `/dev/hidraw5` (interface 03) was readable/writable by the
desktop user, while `/dev/hidraw6` (interface 04) was root-only with no user
read/write access. The installed application udev rule deliberately granted
only interface 03. This is a concrete access problem consistent with the
missing update interface, although browser enumeration must still be verified.

The rule and fixed-purpose helper now grant/activate desktop-seat access for
interfaces **03 and 04**, restricted to USB **1395:018e**. The updated helper
was rebuilt and installed the rule through PolicyKit. Both nodes now pass
read/write access checks, and installed rule bytes match the packaged rule.
`udevadm verify`, Release build, and device-access/firmware/CLI/package tests passed.

The browser must rediscover/re-authorize the interface before another attempt.
A fresh capture is required for any retry. No firmware command was sent by the
investigation or the access helper, and this access correction does not enable
or validate native firmware installation.
