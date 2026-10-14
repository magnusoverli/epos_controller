# Linux mute notifications — 2026-10-14

The notification implementation uses an independent, passive HID input monitor,
host preference store, speech-triggered rate limiter and desktop D-Bus sender.
It does not add notification values to device settings or write HID reports.

## Coordinated Linux evidence

The operator explicitly agreed to a passive capture, then performed the actions
below. Reports came from IMPACT 860T ANC `1395:018e`, USB key `3-1.1`, interface
**03**, `/dev/hidraw9`. The recorder used `O_RDONLY | O_NONBLOCK`; it sent no
output/feature commands and captured no microphone audio. Interface 04 was not
accessible and was not opened or granted additional permissions.

| Capture directory under `captures/linux/` | Reports | SHA-256 of `reports.jsonl` |
| --- | --- | --- |
| `20261014T085946Z-mute-events` | 3 | `06b18f15753d6450a76094251692a5aabe12d3ff76a668bb3bb8a54c5322e420` |
| `20261014T092805Z-mute-events` | 26 | `e3b2232c0c8770422c55d6500b692e54323a99e90e3a6a91385724d9310233c5` |

1. No call/microphone app was active. The operator reported an unmuted baseline,
   pressed the button and observed three purple flashes, with no mute confirmation.
   This is not labeled a successful mute transition. The capture reached its
   ten-minute deadline while Sound settings were being opened.
2. The operator selected EPOS in Linux Sound settings and confirmed its input
   meter responded. A new capture was started.
3. A button press with that meter active produced a silent meter and a spoken
   mute confirmation, as reported by the operator.
4. The operator performed quiet/speaking/quiet periods while muted and reported
   no noticeable warning. Both speaking-state event values appeared in this phase.
5. The operator unmuted and confirmed the meter responded again. Capture stopped.

Phase markers are operator-guided windows, **not exact timestamps of individual
actions**. Some transitions occur before their confirmation marker. The second
capture also includes six short consumer-control reports (`04…`) and two
`[20,f0,2,0,0]` vendor reports; neither is interpreted as a mute/speech event.

`scripts/verify-mute-evidence.py captures/linux` checks immutable capture hashes,
all 23 vendor-event checksums, and the four captured fixtures. Capture material
is local/ignored; fixtures and verification scripts are portable repository files.

## Event envelope and body

The observed report is 107 bytes, with zero padding after its body:

```text
01 eb fe 01 00 01 01 00 00 08 00 CRC 00 | body (8 bytes) | padding
```

Bytes 5/6 distinguish these events from settings replies. The body uses
little-endian 16-bit words and the established CRC-8/07/init-ff.

| Body words | Meaning | Second capture elapsed seconds (example) |
| --- | --- | --- |
| `10,f0,1,1` | Speaking while muted | 80.841304 |
| `10,f0,1,0` | Speaking-while-muted condition ended | 82.491319 |
| `10,f0,2,1` | Microphone muted | 24.601989 |
| `10,f0,2,0` | Microphone unmuted | 25.990997 |

The application accepts only this observed envelope, exact length/body shape,
boolean values, checksum and zero padding. Other vendor events, consumer reports,
settings responses and malformed packets are ignored by the notification decoder.
The settings decoder continues to reject the asynchronous event envelope.

## Independent Windows adapter interpretation

Use the adapter hash/image base from [ADAPTER.md](ADAPTER.md).

- Parser `102f33b0`, at `102f3a9b–102f3aff`, matches `[10,f0,1]`, reads a 16-bit
  value, and stores variant index `0d`. Its log at `104b0a68` identifies
  `SpeakerMic_Events_TalkWhileMuteStatus`.
- `102f3b04–102f3b68` similarly matches `[10,f0,2]`, storing variant index `0e`.
  Log `104b0ae0` identifies `SpeakerMic_Events_MicMuteStateChanged`.
- The caller increments the variant index before dispatch (`10286233–1028623e`).
  Thus table `10281f2c` entries 14/15 call `102808e0`/`10280bb0`.
- `10280956–1028097e` forwards a speaking notification only for value 1.
  `10280bea–10280bf1` maps microphone value 1 to string `muted` at `1047a930`,
  and other values to `unmuted` at `1047a93c`; Linux rejects non-boolean values.
- `[10,f0,4]`, variant `10`, is the **Talk While Muted preference** event, not
  microphone state or speaking detection. The existing scalar preference is
  neither an initial mute-state query nor an event subscription.

## Interval behavior

The Windows adapter's `CMuteNotification` host logic uses a per-device cooldown.
`10151df0–10151e43` launches a worker targeting `101522d0`
(`CMuteNotification::runTimer4muteNotification`). At `10152456–1015246a` the
worker sets a flag true; it waits the configured integer multiplied by
`0x0000000df8475800` = **60,000,000,000 nanoseconds**, then clears the flag at
`10152632–1015263d`. Expiry does not send a notification. The notification handler
checks the flag at `10151bfa/10151c09` and suppresses delivery during cooldown.
This is evidence for minute-based speech-notification throttling, not autonomous
periodic “still muted” reminders.

The Linux policy offers Off / Visual only / Visual and audio, and When I start
speaking / 1 / 5 / 10 minutes. It triggers on speaking-while-muted rising edges,
deduplicates repeated true reports, and applies a monotonic minimum interval.
Mute alone, silence, or interval expiry never generates a notification. A muted
state is not guessed at startup; a speaking-while-muted report carries the needed
condition itself. An unmute report clears the active speech edge but does not
bypass the cooldown. Reconfiguration/reconnection starts a fresh event session.
Exact Windows behavior across preference changes/reconnections remains untested.

## Linux integration and limits

- Notifications default Off. Preferences are saved at
  `$XDG_CONFIG_HOME/headset-controller/notifications.ini`, scoped to model + USB
  port because the app does not collect a serial number. Moving ports uses a
  separate preference entry. These host preferences are not erased by headset
  Factory Reset and do not appear in `--read-settings`.
- While enabled, the selected supported connection is monitored across GUI pages.
  The independent read-only hidraw handle has its own input queue and takes no
  settings-transaction lock. Opened-device identity and descriptor checks are
  shared with the settings transport. The app sends no event-enable commands.
- Selection loss, stream EOF/error, or disabling notifications closes the handle.
  Retry is explicit after an error. Reconnect follows the existing device-selection
  policy. Automation/headless launches never start the monitor or send notifications.
- Visual delivery uses the session-bus `org.freedesktop.Notifications.Notify` method,
  with a 2-second asynchronous deadline and 5-second requested display duration.
  Desktop sound hints proved ineffective on the operator's desktop despite working
  visual popups. Both modes now suppress desktop sound, while Visual and audio
  uses a separate `AlertSound` backend with `pactl`/`paplay` and an original PCM cue.
  USB vendor/product and the selected port identify the unique sink; no guessed
  default output or master volume/mute changes are used. Playback has a four-second
  bound, reports failures, and is cancelled on selection/preference changes.
  `pulseaudio-utils` works with both PulseAudio and PipeWire-Pulse. Popup visibility
  still depends on the desktop service. The direct cue is independent of its hints.
- The app must remain running. Desktop-service failures are reported in Settings;
  old notifications are not replayed, and stale asynchronous completions from a
  retired connection are ignored. Missing HID access uses the existing narrowly
  scoped access setup; no additional privilege or interface permission is added.
- The operator confirmed audible two-note playback through the headset from the
  production `AlertSound` backend on 2026-10-14. Its first test was missed; replay
  after the operator explicitly indicated readiness was heard. A temporary manual
  harness called the production implementation, without HID commands or audio recording.
   The temporary GUI **Test notification** action was later removed during UI polish.
- The operator also confirmed that the rebuilt GUI automatically redetected the
  headset and loaded settings after unplug/replug, without an administrator prompt.

Tests exercise captured bytes, malformed/foreign reports, duplicate events,
minute boundaries, silence/no-reminder behavior, persistence, automation exclusion,
stream disconnect/retry, stale delivery completions, GUI choices, and actual D-Bus
method/hint serialization against a fake notification service on a private bus.
All 11 Release and ASan/UBSan CTest suites pass, along with QML lint,
relocated-install checks and raw-capture verification. The private-bus tests
require `dbus-run-session` and do not send notifications to the operator's desktop.
