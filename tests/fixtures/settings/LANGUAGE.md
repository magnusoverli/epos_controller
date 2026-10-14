# Voice-prompt language — 2026-10-14

The IMPACT 860T ANC now exposes seven language choices, bringing the preview to
19 readable settings plus Factory Reset (20 of 22 device controls). A read-only
Linux run returned `promptLanguage=0` (English). No language change has yet been
performed on the live headset.

## Model-specific capability evidence

Addresses refer to the same PE32 `dciadapter.dll` and SHA-256 documented in
[ADAPTER.md](ADAPTER.md). This mapping is specific to USB product `018e`, not an
assumption that every language known to the adapter is installed on every model.

- `100fca16–100fca56` constructs the product list `0186,0187,0188,018b,018c,018d,
  018e,018f,0191,0192`; `100fca66` attaches its predicate.
- `100fca97–100fcaa6` adds language feature object `105d4df8` to that group.
- `100f6b10–100f6b1a` constructs that feature with mapping vector `105d4ca8`.
- `100f6b30–100f6ba5` initializes **seven** pairs, using constants at `104ba480`,
  `104ba4a0`, `104ba4c0` and a final pair `{0308,7}`. The resulting pairs are
  `{0301,0}, {0302,1}, {0303,2}, {0304,3}, {0305,4}, {0306,5}, {0308,7}`.
- Label initialization at `1000d2e3–1000d41c` identifies these choices:

| UI choice | Adapter feature-value ID | Native value |
| --- | --- | --- |
| English | `0301` | 0 |
| German | `0302` | 1 |
| French | `0303` | 2 |
| Spanish | `0304` | 3 |
| Chinese | `0305` | 4 |
| Japanese | `0306` | 5 |
| Korean | `0308` | 7 |

Russian (`0307`, native 6) is deliberately absent. The adapter also defines
eight-language (`105d4c90`) and three-language (`105d4c9c`) lists for other
product groups. Do not use either of those lists for this headset.

Reproduce the relevant sections without executing the vendor binary:

```sh
objdump -d -Mintel --start-address=0x100fca10 --stop-address=0x100fcaab /path/to/dciadapter.dll
objdump -d -Mintel --start-address=0x100f6b10 --stop-address=0x100f6bb0 /path/to/dciadapter.dll
objdump -s --start-address=0x104ba480 --stop-address=0x104ba4e0 /path/to/dciadapter.dll
objdump -d -Mintel --start-address=0x1000d2e2 --stop-address=0x1000d41c /path/to/dciadapter.dll
```

## Wire commands and status

`1030c490` constructs the language feature with getter vtable `104b830c` and
setter vtable `104b8360`, each using the supplied model-specific mapping.
Getter `103025d0` calls parser slot `40`; setter `103028d0`, at `10302959–10302965`,
maps the chosen ID to its native word and calls parser slot `44`.
In parser vtable `104873f4`, these slots resolve to `102fabd0` and `102fcd00`.
They forward to generated slots `1a0/19c`, functions `10322040/10322320`:

```text
get: [07,03]          -> [07,03,language]
set: [07,02,language] -> [07,02,status]
```

Words are 16-bit little-endian, framed with the established body checksum.
The setter wrapper accepts **only status 0**. Status 1 returns adapter error
`07d8`; other nonzero statuses return `0bc3`. These are not success replies.
The Linux implementation treats any nonzero/missing/malformed status as an
uncertain write outcome, invalidates the session, and requires fresh reads.
It never retries a language change or initiates a firmware/package transfer.

This resolves the earlier ambiguity in [PROMPTS.md](PROMPTS.md): the adapter has
a direct scalar setter for this model's declared choices. Its separate
`enum_vp_components` and firmware-update paths are not prerequisites to exposing
this list, nor proof that arbitrary downloaded languages can be selected.
The product filter for individually upgradeable voice prompts at `102853ee`
uses `1055f550`; its list initialized at `100d9350–100d938d` excludes `018e`.
Firmware updating and language-package installation remain separate future work.

## Captures and verification

Enumeration frames **157/159** capture `[07,03] -> [07,03,0]`, consistent with
the Windows UI's English selection. The same getter repeats at 161/163,
265/267 and 353/355. The raw-capture verifier now checks 32 captured fixture
pairs. Getter and setter fixtures for other languages are explicitly synthetic
predictions, generated independently with polynomial-division CRC calculation.

Tests cover every supported choice with full readback and exact restoration,
rejection of native 6 and unknown write values, lossless unknown readings,
missing/nonzero setter status, extra reply words, retired failed sessions, and
the actual GUI's Korean choice (index 6 maps to native 7).

All 19 fields were read successfully from the Linux headset via
`headset-controller --read-settings`; the local output is retained at
`captures/linux/20261014-settings-extension/readback-language.json`.
Setter change/readback/restore validation remains pending, including whether
language changes affect any currently unimplemented device state.
All 10 Release and ASan/UBSan CTest suites pass, together with QML lint,
clang-format 21, relocated-install checks and raw-capture verification.

## Notification investigation

Follow-up: the operator-assisted Linux capture and host notification implementation
are now documented in [MUTE_EVENTS.md](MUTE_EVENTS.md). The evidence gaps listed
below describe the investigation before that capture.

The two remaining catalog entries are host-notification work. The adapter
distinguishes a **speaking while muted event** from the **Talk While Muted
preference**:

- Internal event index 14 in dispatch table `10281f2c` reaches `102808e0` via
  `10281db4`. It examines a 16-bit talking value and forwards a notification
  only when it equals 1 (`10280956–1028097e`).
- Internal event index 17 reaches `1027ff40`, which updates feature `8100`
  (Talk While Muted setting) from a boolean. It is not a speaking event.
- `AgentApp.dll` at `10096e18–10096ebc` checks notification policy through a
  callback and emits UI message `082d`. The UI's desktop-notification behavior
  is documented in PROMPTS.md.

These are internal callback indices, **not established wire command IDs**.
There is still no validated raw mute/speaking event pair, subscription lifecycle
or initial mute-state query for a Linux monitor. A settings getter returning
`talkWhileMuted=1` must not be mistaken for a currently muted/speaking headset.
Next evidence needed: labeled mute/unmute and speaking-while-muted reports,
their HID interface and framing, and the host interval policy. That will support
a separate event monitor and Linux desktop-notification backend with persistent
per-device preferences; it must coexist with settings transactions.
