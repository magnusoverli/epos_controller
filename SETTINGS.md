# Headset settings: implementation and parity roadmap

## First live-control milestone — 2026-10-13

The first milestone added four settings on the IMPACT 860T ANC (`1395:018e`).
The Windows captures establish wire behavior; the application has its own domain,
transport, orchestration and presentation layers.

| Setting | First-milestone choices | Live Linux validation |
| --- | --- | --- |
| Sidetone | 32% (raw 82), 33% (raw 84) | 82 → 84 → 82 |
| ANC mode | Off (0), Active Noise Cancellation (1) | 1 → 0 → 1 |
| Adaptive ANC | Off (0), On (1), while ANC is on | 0 → 1 → 0 |
| ANC level | Middle (1), High (2), with ANC on and Adaptive off | 2 → 1 → 2 |

All four operator-authorized hardware round trips succeeded, with **exact full
four-value restoration after every test**. Initial and final tuple: sidetone 82,
mode 1, Adaptive 0, level 2. Local JSON logs are in
`captures/linux/20261013T082731Z-settings-validation/`. These are application-level
device readback logs, not independent USB bus captures or acoustic measurements.

Standalone getters and setters worked without replaying the Windows startup
traffic or its unidentified `06 00 2f 00` pre-query. This establishes operation on
the tested connection, not every firmware/cold-start/reconnect scenario. The
Windows bundle identifies firmware 1.5.64; the live Linux firmware getter agrees.

## Expanded controls — adapter evidence

The preview now exposes sidetone **0–100%**, **TalkThrough (2)** and **Low ANC (0)**.
Static inspection traces these value mappings through the Windows adapter to the
same USB command IDs established by the captures. See the reproducible
[adapter findings and derived-fixture provenance](tests/fixtures/settings/ADAPTER.md).
These additional values have automated coverage but **still need Linux hardware
change-and-restore validation**; the table above records the live-tested subset.

Sidetone uses `floor((255*percent + 50)/100)` for writes. Reads retain the exact
native value: 81 displays as `32% (raw 81)` rather than being rewritten to 82.
Out-of-range reads remain visible as unknown. Sidetone is mouse/touch controlled;
releasing the slider saves one change, with no separate Apply button or keyboard
adjustment. Unrelated readbacks preserve an in-progress drag. Explicit refresh
or a new connection invalidates the gesture so its release cannot write stale intent.

Expanded-control checks passed: all nine Release and ASan/UBSan CTest suites,
QML lint, clang-format 21 and relocated-install startup. Tests cover scale
boundaries/rounding, raw-value preservation, TalkThrough dependencies, Low ANC,
unknown readings and reset keyboard interaction at normal/high DPI. The later
[UI polishing review](UI_POLISH.md) covers release-to-save and queued edits.

## Additional preferences — 2026-10-14

The second settings milestone expanded the preview to **12 settings**, adding:

- Answer calls by lowering the boom and mute by raising it.
- Boom arm side (Right/Left).
- No prompts during calls (Off/On).
- Product ringtone (Off/On).
- Busylight (Automatic/Busy/Free).
- Talk While Muted indication (Off/On).
- Button status (Enabled/Disabled).

The eight new controls use captured Windows getters and traced adapter setters;
their **Linux change-and-restore validation remains pending**. The attempted live
read was blocked by missing access to the current interface-03 node. See
[command/value evidence and the remaining-settings backlog](tests/fixtures/settings/EXTENDED.md).
All changes verify the entire 12-value snapshot, including the original controls.
If any read fails, the snapshot stays unavailable instead of inventing defaults;
the error identifies the failing feature. This remains a model-specific preview.

Validation passed: all nine Release and ASan/UBSan test suites, QML lint,
clang-format, relocated installation, and raw-capture checks for all 25 captured
fixture pairs. New setter fixtures are explicitly labeled synthetic predictions.

## Audio limiter and wear detection — 2026-10-14

The third settings milestone expanded the preview to **16 settings**, adding:

- Audio Limiter: Off, EU / US limiter, AU limiter.
- Answer Call — By putting headset on: Off/On.
- Mute Call — By removing headset: Off/On.
- Smart Pause — By taking headset off: Off/On.

All 16 getters succeeded on the Linux headset after access became available.
The new fields read `audioLimiter=0`, `answerOnWear=0`, `muteOnRemoval=0`,
`smartPause=1`. This was read-only; live setter/restore validation remains pending.
The setter implementations use traced Windows-adapter mappings and independent
synthetic fixtures. See [limiter and sensor evidence](tests/fixtures/settings/SENSORS.md).
Each change verifies all 16 fields, including neighboring wear controls.
All 10 Release and ASan/UBSan suites, QML lint, formatting, relocated-install
checks and verification of all 29 captured fixture pairs pass.

## Prompt modes, Auto Pause and Factory Reset — 2026-10-14

This milestone enabled **19 of the 22 device controls:** 18 readable settings and the
separate Factory Reset action. The latest additions are:

- Auto Pause: pause PC music on PC calls (Off/On), distinct from Smart Pause.
- Tone & Voice Prompts: Off, Tone prompts or Voice prompts.
- Factory Reset: explicit confirmation for the selected connection, with fresh
  preflight and resulting-settings readback.

A live Linux read succeeded for all 18 fields, including `autoPause=0` and
`prompts=1` (voice prompts). No live reset or new setter was performed. The new
setters/reset use traced adapter commands and synthetic fixtures and still need
hardware validation. See [prompt/reset evidence](tests/fixtures/settings/PROMPTS.md).
All 10 Release and ASan/UBSan suites pass, as do QML lint, formatting,
relocated-install checks and verification of all 31 captured fixture pairs.

Factory Reset can erase settings beyond those implemented here and interrupt
audio. Cancel has initial focus; selection changes or disconnects invalidate the
confirmation. There is no automatic undo. A timeout or disconnect after output
is reported as an uncertain outcome, never retried automatically. Reconnect,
select and refresh to inspect the headset. An acknowledged reset is followed by
actual readback; the app does not claim a verified factory-default tuple.

The subsequent language investigation is described below. Mute notifications
still need a Linux notification backend and validated mute/speaking events;
Windows evidence includes host-side notification behavior.

## Voice-prompt language — 2026-10-14

This milestone enabled **20 of 22 device controls:** 19 readable settings plus the
separate Factory Reset action. Voice Prompt Language offers English, German,
French, Spanish, Chinese, Japanese and Korean, using the adapter's model-specific
list for product `018e`. Russian is deliberately excluded. Selection uses a
direct setting command, with status-zero acknowledgement and full readback;
it does not install a language package or start a firmware update.

All 19 settings read successfully on Linux, including `promptLanguage=0`
(English). New language setters still need live change/readback/restore
validation. See [language evidence](tests/fixtures/settings/LANGUAGE.md) for
the model filter, mappings, 32 captured fixture pairs and synthetic write fixtures.
All 10 Release and ASan/UBSan suites, QML lint, formatting, relocated-install
checks and raw-capture verification pass.

Mute Notification and Mute Interval were completed in the following milestone.

## Desktop mute notifications — 2026-10-14

**All 22 device-catalog entries now have implementations:** 19 readable device
settings, the separate Factory Reset action and two local notification controls.
This is not full Windows application parity: application preferences, newer-firmware
checking/installation and remaining hardware validation are still pending.

The operator-assisted Linux capture established microphone-mute and
speaking-while-muted reports on HID interface 03. The microphone was confirmed
unmuted again at the end. No device-setting writes or microphone recordings were
performed. See [mute-event evidence](tests/fixtures/settings/MUTE_EVENTS.md).

- **Mute Notification:** Off (initial local default), Visual only, Visual and audio.
- **Mute Interval:** When I start speaking, 1 minute, 5 minutes or 10 minutes.
  The interval is a minimum gap between speech-triggered notifications. Silence,
  muting alone and timer expiry never create reminders.
- Preferences are saved under `$XDG_CONFIG_HOME/headset-controller/notifications.ini`,
  per model + USB port. They are host preferences, not device values, and are not
  erased by headset Factory Reset or included in `--read-settings`.
- Keep the GUI running. The event monitor follows the selected supported headset
  across all pages and uses an independent read-only hidraw input queue. It does
  not lock out settings transactions, send event-enable commands or request more
  interface permissions. Screenshot, smoke-test and headless modes never monitor.
- Visual delivery uses the desktop session's `org.freedesktop.Notifications` service.
  Visual and audio also plays an original two-note cue directly through the selected
  headset using `paplay`/`pactl` (`pulseaudio-utils`, including PipeWire-Pulse).
  Routing matches USB vendor/product and port, and respects output volume/mute.
  Ambiguous/missing audio outputs and playback failures appear in Settings.
   Desktop sounds are suppressed to avoid double playback.

Captured event decoding and private-bus desktop delivery are tested independently.
The operator reported working visual popups, then confirmed hearing the new audio
backend's two-note cue through the headset. Full speech-triggered integration remains
available to check with the updated GUI; direct playback no longer depends on desktop sound hints.
All 11 Release and ASan/UBSan suites, QML lint, relocated-install checks and both
capture verifiers pass (32 settings fixture pairs and four Linux event fixtures).

## Use

Open **Settings → Device settings**: settings load automatically and enable the
controls. Sections start collapsed; click a header or focus it and press Space
to expand/collapse it. Expansion choices are retained while navigating within the
app, including between device and application settings.
Reopening the page or selecting a new connected headset refreshes them.
After reset/reconnect on the same USB port, the previous model/port selection is
restored automatically and settings reload while the page is open. The new connection
ID invalidates old operations and reset confirmations; writes are never replayed.
Moving to a different port still requires selection. **Refresh** remains available,
changing to **Retry** on failure.
Dropdown selection or sidetone slider release updates the control immediately.
One worker serializes changes, retaining only the latest waiting value per setting.
Every dispatched change still gets a fresh preflight and complete 19-setting
readback. Confirmed device values and requested values are separate; there are
no settings progress indicators or success notifications. Dependent controls
follow requested prerequisites, and queued edits are revalidated against confirmed
readback before dispatch. Errors appear at the affected setting; a failed operation
cancels waiting edits and requires a fresh read instead of replaying uncertain writes.

Normal GUI startup on the Device settings page also loads settings automatically.
Diagnostics, screenshot/smoke modes and CTest never initiate settings transactions
on real hardware. Normal GUI access setup also reads settings after authorization.
The display-free read command remains explicit:

```sh
./build/release/headset-controller --read-settings
./build/release/headset-controller --read-settings --device 3-1.1
```

The optional device key comes from `--list-devices`; it is required if multiple
headsets are connected. Output contains **device-native** values or an error;
exit code 4 indicates selection/settings failure. This command sends vendor read
requests inside HID output reports; it is not descriptor-only inspection.

### Linux access

**Reconnect regression validated:** after loading the rebuilt GUI, the operator
unplugged/reconnected the headset on the same port and confirmed automatic detection,
settings loading, and no administrator prompt. No additional live factory reset was
performed by the agent. The earlier reset disappearance was traced to selection
remaining bound to the retired connection ID. Discovery now follows the model/port
and polls every 1.5 seconds as a fallback for ACL updates without udev notifications.

Run the application as your desktop user. Discovery is readable without extra
permissions; settings require read/write access to the **interface 03 hidraw
node**. Its number can change after reconnect. The application revalidates the
opened handle against the discovered connection, USB identity and descriptor.

**Normal GUI launches set up persistent access automatically.** With a supported
headset selected, the app detects missing permissions and opens the system's
one-time administrator authorization prompt, even on the home page, after a
2.5-second permission-settle delay and a fresh scan. No in-app
setup click or terminal command is needed. After approval, the helper installs
the rule, reloads udev, triggers only matching settings interfaces, and the GUI
rescans and reads settings. Rebuilds, reconnects and reboots keep the rule.

This works from the build directory and from a source installation. Build the
`headset-controller` target normally: it also builds `headset-access-helper`.
`cmake --install` includes the helper under `libexec/headset-controller` (or your
configured libexec directory), and the application locates it after relocation.
The GUI runs unprivileged; only this small, non-Qt helper runs through `pkexec`.
It accepts no installation paths or rule text and embeds the supplied rule at
build time. It is not setuid and installs no passwordless PolicyKit policy.

The desktop needs **pkexec/PolicyKit, an authentication agent, udev and active-seat
ACL support** (normally systemd-logind). Administrator authentication cannot be
silently bypassed. A native system package can install the rule during package
installation to avoid a first-launch authentication prompt.

Cancelling or failing authorization suppresses further automatic attempts across
launches. The GUI then offers **Retry access setup**. The per-user preference is
stored in `$XDG_CONFIG_HOME/headset-controller/access-setup.ini` (normally under
`~/.config`). If activation is incomplete, the GUI explains whether to reconnect
or restart. Headless commands, screenshot mode and smoke tests never initiate
authorization. Automated tests use an unprivileged fake helper and temporary
directories; they do not install or reload host rules.

Automatic setup passes the Release and ASan/UBSan test suites (10 suites each),
rule validation, QML lint, formatting and relocated-install checks. The real
desktop authentication prompt and live post-install access still need interactive
verification.

For administrator-managed or headless installations, install the rule manually:

```sh
sudo install -m 0644 packaging/70-headset-controller.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

After manual installation, reconnect the headset. The rule grants `uaccess` only to
`1395:018e` interface 03; it does not make all HID nodes world-writable. In a staged
installation the rule is also supplied under `share/headset-controller/`. Building
and staging alone do not modify host rules; a normal GUI launch starts setup if
access is needed. Remote or
headless sessions need administrator-managed access because `uaccess` follows the
active desktop seat.

Close competing headset-control software. The transport takes a nonblocking
advisory lock, which protects cooperating instances of this app; it cannot exclude
software that ignores that lock. It keeps the standard USB audio/HID drivers
attached. The kernel must implement `HIDIOCSOUTPUT`; unsupported kernels report an
error rather than substituting another USB operation.

## Architecture

1. **`SettingsTypes`**: typed features, lossless raw values, requested changes,
   errors, supported values and dependencies. No Qt or packet offsets.
2. **`EposProtocol`**: bounded, Qt-free serialization/parsing for the evidenced
   settings subset. No filesystem, devices, QML, traces or Windows components.
3. **`SettingsSession`**: semantic reads and changes over an injected report
   transport. Re-reads before a write, rejects stale expected values, matches
   logical replies, and verifies the complete settings snapshot afterward.
4. **`ReportTransport` / `LinuxHidTransport`**: generic report byte I/O with an
   owned descriptor. Select/revalidate interface 03, use EP0
   `HIDIOCSOUTPUT` (Output SET_REPORT), and read interrupt-IN through hidraw.
5. **`SettingsController`**: a Qt presentation adapter and one owned worker.
   Only one operation can run at a time. Connection/selection changes cancel it
   and invalidate its generation; late results cannot populate another device's
   UI. QML receives current values/options/status, not protocol details.

Errors discard cached readback; another successful read is required before a new
write. Automatic reads run once per page activation or changed selection, not on
unchanged discovery polls. Errors do not cause a retry loop. A replacement read
waits for any cancelled operation to finish; leaving the page clears pending reads.
Failed writes are never automatically retried or blindly rolled back.
Transport completion alone is not proof of setting application. A failure after
output begins is explicitly marked as potentially applied.

Reads have a two-second matching-reply deadline, a 128-report budget and
cancellable polling in slices of at most 25 ms. Opening requires a quiet receive
window with a bounded drain. Output uses the kernel's synchronous USB control
transfer timeout on the worker: cancellation cannot interrupt an in-flight ioctl,
and shutdown waits for it. The input deadline is an application policy, not a
vendor-specified timeout. No protocol transaction ID has been established; the
advisory lock, fresh handles and drain do not prove exclusion of arbitrarily late
replies or non-cooperating controllers. Reconnect after repeated timeouts instead
of repeatedly issuing changes.

Firmware updating uses a separate service/state machine. Runtime call-control
events and persistent device preferences also remain separate concerns. Neither
Windows process names nor its IPC/configuration objects define these boundaries.

## Protocol findings and independent tests

Observed transport: USB interface 3, report ID 1, **107 bytes** including ID.
Host requests are Output SET_REPORT with setup `21 09 01 02 03 00 6b 00`;
logical replies arrive separately on endpoint `81`. The codec checks complete
length, header, body size, checksum and zero padding before exposing a value.

Offsets include the report ID:

- Body length at 9–10, little-endian; supported settings messages use 4, 6, 8 or 10.
- Checksum at 11, reserved zero at 12: **CRC-8, polynomial 0x07, initial 0xff,
  non-reflected, xorout 0**, over the body starting at 13.
- Group/operation at 13–16 and optional little-endian payload words at 17–18
  and subsequent pairs; boom-action writes carry a selector followed by the value.
  Wear-detection messages carry subcommand, selector and query/write mode.
- Getter/setter groups and operations: sidetone `0a:0c/0b`, ANC `17:20/1f`,
  Adaptive `17:22/21`, level `17:1a/19` (hexadecimal).

The checksum rule matches **342 reports / 93 distinct reports** from the raw
Windows captures, including enumeration and reconnect traffic. This is broader
validation than the settings-only test subset. Unknown operations are not exposed
as callable APIs. Write replies have captured shape checks, but their zero slots
are not asserted to be documented success codes.

The tracked [fixture provenance](tests/fixtures/settings/README.md) and
`scripts/verify-settings-evidence.py` connect tests to raw evidence. Builds use
small independent packet fixtures, never the ignored capture folder. Tests cover
byte-exact encoding, malformed/truncated responses, wrong-operation input,
readback mismatch, dependencies, concurrent state changes, permission failure,
cancellation, stale selection results and GUI interaction. Simulated replies use
immutable captured and explicitly labeled derived bytes, never values encoded
with the production codec.

Milestone checks passed: all nine Release and ASan/UBSan CTest suites, parser-only
build/test, QML lint, clang-format 21, udev rule validation, and relocated-install
diagnostics/all-page startup. UI tests explicitly wait for the virtualized setting
row to be laid out before interacting with it.

For a future **explicitly authorized, idle-headset** hardware test:

```sh
cmake --build build/release --target headset-settings-probe
./build/release/headset-settings-probe --device 3-1.1 --feature sidetone --value 84 --round-trip
```

This manual-only, uninstalled target verifies the change and restores the exact
original value with another readback. It refuses a test when the original value
has no supported setter. On failure it stops, reports restoration status and
requires inspection before further changes; it does not reset the headset.

## Next steps toward Windows parity

1. Validate the expanded sidetone range, TalkThrough and Low ANC on hardware;
   test fresh connections, physical hotplug and supported firmware versions.
2. Validate the 15 additional preference setters and separate reset workflow
   on hardware, plus the hardware-to-desktop notification flow and reconnect behavior.
   Trace shared selectors, reply statuses, value units and dependencies before
   exposing each remaining control.
3. Firmware identity, public catalog discovery and a capture-backed native installer
   are implemented, including user-initiated future production releases without a
   version allowlist. Validate native flashing on hardware and further investigate
   nonzero offsets, validation-pending behavior, ownership and broader recovery. See the
   [successful Web capture and implementation limits](tests/fixtures/firmware/WEB_SUCCESS.md).
4. Complete application preferences, applicable call-control integrations and
   device-management workflows, plus native desktop and distribution validation.

Each feature needs valid input choices, a device read path, verified writes,
dependency behavior, failure handling and hardware validation before being
presented as supported. See the local `PLAN.md` for detailed research tasks.
