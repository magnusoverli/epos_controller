# Prompt modes, Auto Pause and Factory Reset — 2026-10-14

The preview now has 18 readable settings and a separate Factory Reset action
(19 of the 22 catalog entries). Both new getters succeeded on the Linux headset:
`autoPause=0`, `prompts=1`. No live setter or factory reset was performed.
New writes and the reset action still require hardware validation.

## Inputs and reproduction

All adapter addresses below use the PE32 image base and SHA-256 in
[ADAPTER.md](ADAPTER.md). The enumeration capture identity is in
[EXTENDED.md](EXTENDED.md). For example:

```sh
objdump -d -Mintel --start-address=0x1030d5b0 --stop-address=0x1030d670 /path/to/dciadapter.dll
objdump -s --start-address=0x104ba490 --stop-address=0x104ba4c0 /path/to/dciadapter.dll
```

There are now 31 captured fixture pairs. `verify-settings-evidence.py` checks
each newly cited frame against raw USBPcap data, not just extracted JSON.
Additional setter/value fixtures use the independent polynomial-division CRC
method from ADAPTER.md and remain labeled synthetic.

## Tone & Voice Prompts

The three-way feature must not be confused with the older voice-prompts
boolean. The complete mapping is:

| UI choice | Adapter feature-value ID | Native value |
| --- | --- | --- |
| Off | `0341` | 0 |
| Tone prompts | `0342` | 2 |
| Voice prompts | `0343` | 1 |

Label initialization at `1000c3c0–1000c40b` identifies these IDs. The mapping
vector at `10562648` points to `105d445c..105d4484`. Its initializer at
`100f73c0` copies the pairs `{0341,0,0342,2}` from `104ba490` and
`{0343,1,0344,4}` from `104ba4b0`, then appends `{0345,5}`. Getter
`1030bc60` and setter `1030d5b0` search this vector; the latter forwards the
native value through parser slot `d8`. The implementation exposes only the
three choices established for this headset, not modes 4/5 for other variants.
Unknown native readings remain lossless instead of being normalized to Off.

Parser slots `d4/d8` route to `102f7ab0/102fbb20`, forwarding through generated
slots `1a4/198` to `10321c20/103221a0`:

```text
get:       [07, 05]        -> [07, 05, mode]
set:       [07, 01, mode]  -> [07, 01, status]
```

All words are little-endian 16-bit words. The setter wrapper requires status 0.
Enumeration frames **249/251** read native 1 (voice prompts). Tests cover every
choice, nonzero setter status and the GUI's non-sequential tone/voice mapping.

## Auto Pause

This is PC-music pause on PC calls, distinct from wear-triggered Smart Pause.
The native values are 0=Off, 1=On. Label initialization at `1000dfe3–1000e01a`
ties Enabled/Disabled to `8221/8222`. The setter at `10301d45–10301d79` maps
feature IDs `8221/8222` to 1/0 and calls parser slot `184`. Wrapper `102f7180`
forwards to generated slot `2b0` (`1031d6c0`). Getter wrapper `102f7bf0`
uses generated slot `2b4` (`1031dc00`) and accepts only 0/1.

```text
get:       [01, 30]         -> [01, 30, value]
set:       [01, 2f, value]  -> [01, 2f]
```

Enumeration frames **337/339** read native 0. These requests are captured on
the IMPACT 860T ANC, rather than inferred solely from an adapter method name.

## Factory Reset action

The generated `Service_FactoryReset` function at `1031dae0` reserves four-byte
request/reply bodies and sends `[01,13]`, expecting `[01,13]`. Parser wrapper
`102f7330` (slot `e4` in `104873f4`) forwards to generated slot `28c`.
The distinct `Service_ProductionReset` command `[01,1a]` is not used.

`SettingsActionFixtures.h` contains a synthetic reset pair. There is **no live
reset capture, validated factory-default tuple, or verified reboot timing**.
The application therefore reports acknowledgement and actual post-action
readback, not proof that every device setting equals a known factory default.

The action is separate from scalar features and the manual setting round-trip
probe. The GUI requires a loaded snapshot and explicit confirmation for the
selected connection. Cancel has initial focus. Disconnecting/changing selection
closes the dialog, and the controller checks the confirmed connection ID again.
The session re-reads settings and rejects stale preflight data before sending
one reset. It then validates the reply shape and reads all 18 settings anew.

If output, acknowledgement or post-reset readback fails, the state is unknown
and the app reports that reset may have applied. It never retries reset or
attempts a blind rollback. Reconnecting/selecting the headset and refreshing
performs reads only. A reboot that drops the reply is not treated as success.
The confirmation explains that unsupported settings can also be erased and audio
can be interrupted. Startup and discovery never reset the device; tests send
reset reports only to the fake transport.

Tests use a deliberately chosen fake post-reset snapshot, not purported real
factory defaults. Coverage includes stale/cancelled preflight, output failure,
lost/malformed replies, post-reset disconnect, duplicate invocation, stale
connection confirmation, GUI cancel, GUI disconnect, confirmation and readback.
All 10 Release and ASan/UBSan suites pass, along with QML lint, clang-format 21,
relocated-install checks and verification of all 31 captured fixture pairs.
The UI suites also verify dropdown identity across scrolling; the small settings
catalog no longer recycles a dropdown into a different feature while it has focus.

## Remaining three catalog entries

Follow-up: [LANGUAGE.md](LANGUAGE.md) resolves the model-specific language list
and direct selection path. Language selection is now implemented; the two
notification controls were subsequently implemented using [MUTE_EVENTS.md](MUTE_EVENTS.md).

- **Voice Prompt Language:** `[07,03]` reads the current language; selecting a
  language can cross into firmware/language-package installation. Wrapper
  `102fcd00` maps a nonzero `[07,02]` reply to an additional action/error; the
  adapter also contains `query_current_vp_component`, `enum_vp_components` and
  `set_active_voice_prompt_language`. The set of installed/selectable languages
  and package fallback still need establishing before offering choices.
- **Mute Notification / Mute Interval:** the Windows UI explicitly handles
  `MUTE_NOTIFICATION` messages with an `AudioOnly` field, then creates a desktop
  `Notification` with sound on/off. This is evidence of host behavior, not a pair
  of HID setting commands. Linux needs a notification backend, persistent
  per-device preferences, and validated mute/speaking events, including interval
  timing and dependencies. No fake device values or guessed commands are exposed.

The UI evidence above is `dist/browser/main-KEYLTUWD.js`, SHA-256
`69c28c14a1be0b8d892983201c320e5eb68ccb84053201439f6c979149d3bc90`.
Search `getMuteNotification`, `MUTE_NOTIFICATION` and `Mute_Notification_Msg`
to reproduce the host-notification path. Adapter label strings list Off,
Visual Only, Visual and Audio; interval labels include speaking-triggered,
one-, five- and ten-minute modes. Those labels alone do not establish the
underlying event behavior. None of these vendor binaries/scripts is a runtime
or build dependency of this application.
