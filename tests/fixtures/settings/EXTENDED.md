# Additional preference controls — 2026-10-14

Eight preferences are implemented using **captured getters** and **statically
traced setters**. Their Linux change-and-restore tests remain pending. An attempted
12-setting Linux read on 2026-10-14 failed to open interface 03 (`/dev/hidraw9`)
with permission denied; no configuration was changed.

## Sources and encodings

The adapter identity and address convention are recorded in [ADAPTER.md](ADAPTER.md).
All generated-parser functions below are in that same SHA-256-pinned binary.
The captured reads come from `00-enumeration-reviewed.pcapng`, SHA-256
`70524ea8835028a7550504ebe3283a3a40ef4956191067073ea9e2a776beddcd`.

| Preference | Native choices | Group:get/set (hex) | Getter/setter addresses |
| --- | --- | --- | --- |
| Boom arm side | Right=0, Left=1 | `10:2e/2d` | `10320ee0` / `10321300` |
| No prompts during calls | Off=1, On=0 | `07:0a/09` | `10321ee0` / `10321ae0` |
| Product ringtone | Off=0, On=1 | `09:06/05` | `103224a0` / `103228c0` |
| Busylight | Automatic=0, Busy=1, Free=2 | `14:07/06` | `1031d1a0` / `1031d300` |
| Talk While Muted | Off=0, On=1 | `10:30/2f` | `103211a0` / `10321580` |
| Button status | Enabled=0, Disabled=`ffef` | `08:28/27` | `1031c070` / `1031baa0` |
| Answer by lowering boom | Off=0, On=1; selector=0 | `08:1e/1d` | `1031bf00` / `1031b940` |
| Mute by raising boom | Off=0, On=1; selector=1 | `08:1e/1d` | `1031bf00` / `1031b940` |

Bodies are little-endian 16-bit words. The first six getters have a four-byte
body `[group, operation]`; setters have six bytes `[group, operation, value]`.
Boom-action getters have six bytes `[08, 1e, selector]`; setters have eight
bytes `[08, 1d, selector, value]`. All eight getters reply with six-byte bodies
`[group, operation, value]`; all eight setters have four-byte reply bodies
`[group, operation]` in the adapter serializer. Setters are followed by full
readback rather than treating a transport response as proof of application.

### Value mapping details

- **Boom side:** label initialization maps Left/Right to `0931/0932`.
  `10303e6e–10303e94` maps those IDs to native 1/0 and parser slot `1d8`.
  Wrapper `102fbdd0` preserves 0/1 and forwards to generated slot `f4`.
- **In-call prompts:** `8041/8042` are suppression Off/On. At
  `103042de–1030430e` they become native 1/0, respectively. The underlying
  operation enables product sounds, hence the inversion. Wrapper `102fcc20`
  forwards the value through generated slot `1ac`.
- **Ringtone:** `8201/8202` are Off/On; `10301255–103012dc` maps them to
  0/1 and parser slot `26c`. Do not invent additional ringtone choices from
  the native operation's ringtone-type name.
- **Busylight:** IDs `8191/8192/8193` are Auto/Busy/Free. The setter at
  `103008c5–10300976` maps them to 0/1/2.
- **Talk While Muted:** `8101/8102` are On/Off. At
  `1030519e–103051ce` they become native 1/0 and parser slot `218`.
  The generated command controls the talk-while-muted LED indication.
- **Buttons:** `8241/8242` are Enabled/Disabled. `103016f4–10301766` maps
  them to `0000/ffef`; wrapper `102f6bb0` forwards the complete mask through
  generated slot `184`. Neither `0001` nor `ffff` is substituted.
- **Boom actions:** label initialization at `1000dac5–1000db06` identifies
  mute On/Off as `8051/8052` and answer On/Off as `8061/8062`.
  `103044fe–10304532` sends selector 1 for mute; `1030488e–103048c2`
  sends selector 0 for answer. Both use 1=On, 0=Off and parser slot `1f0`.
  Wrapper `102fbd50` forwards both arguments to generated slot `160`.

The generated vtable is at `104ba7d8`; the wrapper vtable is at `104873f4`.
Function names, argument forwarding, request/reply sizes and captured getter
bodies provide independent checks on the routes.

## Fixtures and limitations

`SettingsEnumerationFixtures.h` records eight packet prefixes with original frame
numbers. Zero padding extends each to 107 bytes. `verify-settings-evidence.py`
verifies all 25 captured pairs and explicitly matches these eight pairs against
their cited raw frames.

Additional reads and new setters in `SettingsDerivedFixtures.h` are **synthetic
predictions**, calculated with the independent polynomial-division CRC method in
ADAPTER.md. They are not counted as captured evidence. Tests exercise every new
setter choice, exact restoration and all 12 fields after writes. GUI tests
exercise the inverted prompt choice and 16-bit button mask through dropdowns.

Boom-action replies do not echo the selector. One in-flight exchange and full
readback prevent overlapping operations in this controller, but cannot identify
arbitrarily late replies or exclude other applications. The existing transport
limitations and no-automatic-write-retry rule still apply.

## Remaining settings investigation

Follow-up: audio limiter and the three wear controls are now implemented; all
16 getters succeeded on Linux. See [SENSORS.md](SENSORS.md) for the new command,
value and status mappings. Setter/restore validation remains pending.
Auto Pause, prompt modes and a separate confirmed reset action are also now
implemented; see [PROMPTS.md](PROMPTS.md). All 18 getters succeeded on Linux.

| Remaining setting | Next evidence needed |
| --- | --- |
| Mute notification and interval | Implement Linux host notifications with validated mute/speaking events, interval units, persistence and dependencies. |

The notification controls were subsequently implemented using the captured events
and host policy documented in [MUTE_EVENTS.md](MUTE_EVENTS.md).
Firmware updating continues as its own milestone.
Voice-prompt language is now implemented using the model-specific direct
selection path documented in [LANGUAGE.md](LANGUAGE.md).
