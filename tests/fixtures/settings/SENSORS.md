# Audio limiter and wear detection — 2026-10-14

Four additional controls bring the implemented settings to 16. Getters have
Windows capture evidence and **successful live Linux readback**. Setters are
statically traced predictions; live Linux change-and-restore validation is pending.

## Reproducible inputs

Use the same adapter binary, image base and SHA-256 documented in [ADAPTER.md](ADAPTER.md).
All addresses below are virtual addresses in that binary. Inspect without
executing the vendor software, for example:

```sh
objdump -d -Mintel --start-address=0x1031c950 --stop-address=0x1031caf0 /path/to/dciadapter.dll
```

The enumeration capture and its SHA-256 are documented in [EXTENDED.md](EXTENDED.md).
The original raw frame numbers appear beside the immutable packet prefixes in
`SettingsEnumerationFixtures.h` and are verified by `verify-settings-evidence.py`.
All 29 captured fixture pairs match raw captures. Additional values and setter
fixtures in `SettingsDerivedFixtures.h` are synthetic, not captured transactions.
Their CRC bytes use the independent polynomial-division method from ADAPTER.md.

## Audio limiter

- Command group `0a`, getter `15`, setter `14`.
- Label initialization at `1000bc22–1000bc77` establishes Off=`0411`,
  EU/US limiter=`0412`, AU limiter=`0413`. The getter at `103057e0–1030589a`
  maps native 0/1/2 to those IDs. The setter at `10305a40–10305a9f` maps
  the IDs back to native 0/1/2. No acoustic thresholds are inferred from labels.
- Parser vtable `104873f4`, slots `f4/f0`, routes to getter `102fab60`
  and setter `102fccb0`, respectively. They forward through generated vtable
  `104ba7d8`, slots `128/124`, to `103200e0/10320c30`.
- Getter request: `[0a, 15]`. Reply: `[0a, 15, status, value]`.
  The wrapper explicitly rejects nonzero status before publishing the value.
- Setter request: `[0a, 14, value]`. Reply: `[0a, 14, status]`.
  The setter wrapper likewise requires status 0.
- Enumeration frames **217/219** establish status 0, value 0. Unknown native
  values remain lossless; a failed status is never presented as a setting value.

## Wear detection

| Control | Native selector | Adapter label IDs (On/Off) | Mapping setter |
| --- | --- | --- | --- |
| Mute by removing headset | 0 | `8001/8002` | `10303160–103031ab` |
| Answer by putting headset on | 1 | `8011/8012` | `10303640–1030368b` |
| Smart Pause by taking headset off | 2 | `8021/8022` | `103039d0–10303a1b` |

Label initialization at `1000d863–1000d908` ties these IDs to their presentation
names. Smart Pause specifically uses `By_Taking_the_Headset_Off_Enabled_Osbourne`
and `By_Taking_the_Headset_Off_Disabled_Osbourne`, initialized at `10002c58` and
`10002c18`. Each mapping sends 1 for On, 0 for Off. The three getters at
`10302e90`, `10303400`, `10303810` use selectors 0, 1, 2, respectively.

Parser slots `1b0/1b4` route to `102fac20/102fcdb0`. The getter supplies mode
**2** (query); the setter forwards mode **0/1** (Off/On). Both use generated
slot `190`, function `1031c950` (`Input_Proximity_ConfigureMMI`).

All words below are little-endian 16-bit values:

```text
read:  [08, ef, 0a, selector, 2]
write: [08, ef, 0a, selector, value]
reply: [08, ef, 0a, output]
```

The generated serializer reserves ten request bytes and eight reply bytes, and
reads the output at byte 6 of the reply body. Captured getters **225/227**
(answer), **233/235** (mute), **309/311** (pause) return 0, 0, 1.
The response includes the subcommand but does **not** echo the selector.

The wrapper publishes output as the value for a query, but ignores it after a
write. Accordingly, setter fixtures use a predicted output 0; the implementation
accepts the boolean output domain 0/1 and establishes success only by subsequent
full readback. It does not assume the output echoes the written value. Unknown
subcommands, extra words and non-boolean setter outputs fail the operation.
Mode 2 is not a writable UI value. No sensor mode or unrelated selector is set.

## Verification and limitations

Tests cover every setter choice and exact restoration, all 16 readback fields,
nonzero limiter status, wrong proximity subcommand, malformed setter replies,
ignored writes, and changes to neighboring wear settings. Malformed replies
retire the session. GUI tests operate all four dropdowns and check their values.
All 10 Release and ASan/UBSan suites pass, as do QML lint, clang-format 21,
relocated-install checks and raw-capture verification of all 29 captured pairs.

Live `headset-controller --read-settings` on USB key `3-1.1` succeeded for all
16 fields. New values were `audioLimiter=0`, `answerOnWear=0`, `muteOnRemoval=0`,
`smartPause=1`. This was read-only: no live setter or restoration test was run.
The local CLI output is retained under
`captures/linux/20261014-settings-extension/readback.json`; it is not a USB trace.

Shared proximity replies cannot identify arbitrarily late replies from another
selector or another controller. Serial exchanges, no automatic write retries
and full readback mitigate this limitation without claiming a transaction ID.
Remaining prompt modes/languages, Auto Pause, mute notifications/interval and
factory reset retain the evidence/workflow requirements in EXTENDED.md.
