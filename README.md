# Headset Controller for Linux

An independent C++23 / Qt Quick application for the EPOS IMPACT 860T ANC.

## Status

**Version 1.0.0** is the first source release for Linux and the EPOS IMPACT 860T ANC
(`1395:018e`). See the [release notes](releases/1.0.0.md) and
[GitHub release](https://github.com/magnusoverli/epos_controller/releases/tag/v1.0.0).

Included features:

- Asynchronous USB discovery through Linux sysfs, with debounced udev hotplug events and fallback polling.
- HID interface inspection and JSON diagnostics, available in the GUI or without a display server.
- Live sidetone and ANC settings, asynchronous changes with device readback, and explicit unsupported controls.
- Eight additional preview controls for boom behavior/side, prompt suppression, ringtone, Busylight, Talk While Muted and buttons, backed by captured reads and traced setters; hardware validation is pending.
- Audio limiter, wear controls, Auto Pause, prompt modes and seven voice-prompt languages bring the total to 19 readable settings, plus a confirmed Factory Reset action. All getters have successful Linux readback; newer setters and reset still need hardware validation.
- Mute Notification and Mute Interval now use captured Linux headset events and desktop notifications. All 22 device-catalog entries have implementations; application preferences remain unfinished.
- Firmware version loads automatically on normal GUI connections and appears on Home, Device settings and Updates; `--read-firmware` also reads it headlessly. Captured readback confirms **1.5.619** after the user's successful vendor Web update.
- Offline `--inspect-firmware` checks an official ZIP against an exported catalog offer, including SHA-256 values, manifest, container bounds and numeric version comparison. The Windows handoff establishes an offer for **1.5.619**; this is historical evidence, not a live Linux update check.
- The app automatically checks EPOS's live production-release catalog and offers user-initiated installation of newer official firmware using the captured update flow. No per-version application update or allowlist is needed.
- The selected headset now returns automatically after reset/replug on the same USB port, with fresh settings and firmware reads. Temporary access delays are rechecked before requesting authorization.
- Home, Updates, Settings and Help screens.

Sidetone 32/33%, ANC Off/On, Adaptive Off/On and Middle/High ANC level have passed live Linux change-and-restore tests on `1395:018e`. The app also offers a 0–100% sidetone slider, TalkThrough and Low ANC using traced Windows-adapter mappings; these expanded values still need hardware validation. Other device preferences and broader firmware transitions remain pending. See [settings support, architecture and parity roadmap](SETTINGS.md).

Device settings load automatically when you open the page or select a connected headset while it is open. Refresh/Retry remains available; changes require user interaction. Discovery, diagnostics and screenshot/smoke modes remain descriptor-only. Firmware is reported as unknown rather than inferred from the USB revision. The first discovered headset is initially selected; the selector allows choosing another. After removal/replacement, select the new connection explicitly; its settings load automatically on the open page. Selection never silently follows a different physical connection at the same USB port.

The project-owned source is MIT licensed; see [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).

This is an independent project, not an official EPOS application. No proprietary EPOS SDK is required, linked or included.

## Requirements

- Linux with sysfs.
- A C++23 compiler and standard library supporting `std::expected` and `std::move_only_function`; GCC/libstdc++ 15.2 is tested.
- CMake 3.24 or newer and Ninja.
- Qt 6.12 or newer with Core, Gui, Qml, Quick, QuickControls2, Network and DBus development components, plus QtQuick Controls, Layouts, Window and WorkerScript runtime modules.
- A working Qt TLS backend, compatible OpenSSL runtime and system CA certificates for HTTPS update checks.
- libudev and libarchive development files (`libudev-dev`, `libarchive-dev` on Debian/Ubuntu), and pkg-config.
- Qt Test, Python 3 and `dbus-run-session` for the default test-enabled builds (use `-DBUILD_TESTING=OFF` for an application-only build).
- Native graphics development dependencies, including OpenGL/EGL and the dependencies required by your Qt installation.

The tested toolchain is Qt 6.12.0, CMake 4.4.4, Ninja 1.13.2 and GCC 15.2 on Ubuntu 26.04 x86-64. The supported Qt minimum is deliberately aligned to the verified SDK, replacing the unverified 6.8 claim. CMake checks the required C++23 library features. CI also defines a GCC 14 build and a Qt-independent Clang/libc++ parser/fuzzer build. Distribution packages may be older than these requirements; install a coherent Qt SDK and a suitable compiler if needed.

## Build and run

After cloning the repository, run these commands from its root:

```sh
cmake --preset release
cmake --build --preset release --parallel
./build/release/headset-controller
```

If Qt is installed outside the system search paths, specify its installation prefix when configuring:

```sh
cmake --preset release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
```

Run the application as your normal user. On a normal GUI launch, missing headset access automatically opens a one-time system administrator prompt, installs a narrowly scoped persistent udev rule, and reloads settings. This works with development and source builds too; no terminal setup is normally needed. It requires pkexec/PolicyKit and a desktop authentication agent. Cancellation is remembered, with a **Retry access setup** action available. See [device permissions](SETTINGS.md#linux-access) for details and headless setup. Building does not download dependencies or activate host rules. A clean-machine build remains to be validated; the commands above assume the dependencies are already installed.

### Tests and QML lint

```sh
ctest --preset release
cmake --build build/release --target headset-controller_qmllint
```

Tests include discovery errors/limits, cached HID inspection, connection generations, asynchronous refresh coalescing, timer behavior, descriptor boundaries, headless CLI grammar, and fixture-driven UI interaction at normal/high DPI. UI tests cover lazy pages, keyboard activation/focus, scrolling, plain-text names, selection/disconnect state, and diagnostic snapshot copying. The `dev` preset provides a Debug build. For AddressSanitizer and UndefinedBehaviorSanitizer checks with GCC/Clang:

```sh
cmake --preset sanitize
cmake --build --preset sanitize --parallel
ctest --preset sanitize
```

Settings tests additionally cover independent captured packets, verified readback, dependencies, failures, cancellation and GUI control. Automated tests never change a physical headset.

### Command-line diagnostics

```sh
./build/release/headset-controller --list-devices
./build/release/headset-controller --diagnostics
./build/release/headset-controller --read-settings
./build/release/headset-controller --help
```

`--list-devices` and `--diagnostics` work without a display server. Diagnostics may contain device identifiers and local paths; review them before sharing.

`--read-settings` also works headlessly but sends settings-read commands and requires HID access. It prints native values for the 19 supported fields. Use `--device <USB-key>` when multiple headsets are connected. A settings failure exits with code 4; it is not reported as a successful empty read. Factory Reset is a separate confirmed GUI action and is never triggered by this command.

**Desktop mute notifications:** choose Visual only or Visual and audio under Device settings and keep the app running. The interval limits how frequently speaking while muted can trigger a notification; it does not generate periodic reminders. These preferences default Off and are saved locally per model/USB port. Visual delivery uses `org.freedesktop.Notifications`. Audio is an original two-note cue played directly through the selected headset using `paplay`/`pactl` (`pulseaudio-utils`, also supported by PipeWire-Pulse), respecting its output volume/mute. The app reports unavailable audio outputs/services rather than silently requesting a desktop sound hint. See [event evidence and limitations](tests/fixtures/settings/MUTE_EVENTS.md).

`./build/release/headset-controller --read-firmware` prints the actual device firmware version. See [firmware evidence and remaining update work](tests/fixtures/settings/FIRMWARE.md).

Offline package inspection works without a connected headset or display:

```sh
./build/release/headset-controller --inspect-firmware /path/to/firmware.zip \
  --firmware-offer /path/to/headset-offer.json --installed-version 1.5.64
```

`--firmware-offer` accepts one exported `IMPACT_860T_ANC` offer, not a whole catalog response.
The optional installed version is a caller-provided comparison value, not a fresh device read.
JSON reports matching package/member hashes and structure; failure exits with code 4.
Matching an editable offer does not verify a publisher signature, device compatibility or
catalog freshness. Inspection does not install firmware. See the [handoff review](tests/fixtures/firmware/HANDOFF.md).

### Responsive settings

Settings remain editable while changes are verified in the background. Dropdowns
save on selection; sidetone saves on mouse/touch release, with no Apply button or
keyboard adjustment. See [UI polish and root causes](UI_POLISH.md).

### Automatic firmware checks

Connect the headset: the app reads its installed firmware, queries EPOS's public
production catalog and displays a notice if a newer release exists. **Updates**
shows installed/latest versions, status and the last successful check time, with
**Check for updates** for a manual refresh. Checks repeat every six hours and
on a new connection/changed firmware identity. No saved offers or local ZIPs are
needed in the GUI.

The request sends the fixed product key and EPOS Connect Web's public tenant ID,
not device serial numbers, settings or captured credentials. It uses verified
HTTPS, no redirects, a 1 MiB response limit, a 15-second inactivity timeout and
a 60-second overall deadline. Empty, malformed, failed or conflicting responses
are errors, never “up to date.” Disconnecting invalidates the result. Screenshot
and smoke modes do not make network requests. See [catalog evidence](tests/fixtures/firmware/CATALOG.md).

### In-app installation

For a newer official IMPACT 860T ANC release, choose **Install update** and confirm.
Close EPOS Connect Web and other headset controllers first. Keep the headset
connected and the app open through reboot and final version verification.
Downloads use verified HTTPS and the official catalog's SHA-256 hash; settings
operations pause during installation. Linux login1 must grant the sleep/shutdown
inhibitor. There are no saved-offer or local-file installation controls.

Installation follows the [successful captured Web sequence](tests/fixtures/firmware/WEB_SUCCESS.md),
checks fresh firmware/preflight replies, and reacquires only the same USB serial
and port after reboot. Uncertain results persist in the app data directory under
`firmware/` and block native retries across restarts. Use EPOS recovery/support
if an update is interrupted; there is no automatic resume or rollback.

Future production releases use the same download, transfer, reboot, commit and
version-verification flow without a version allowlist. The package must match
the selected model/version and application-update format; reinstalls and
downgrades remain disabled. The engine passes byte-for-byte replay of the vendor's
successful update and synthetic future-release tests. Native flashing itself
has not yet been hardware-tested.

The [non-flashing protocol investigation](tests/fixtures/firmware/PROTOCOL_RESEARCH.md)
documents Linux-side research of the Windows and Web updaters, including the
remaining transfer/validation ambiguities and a reproducible offline extractor.
The access helper now grants this model's settings (03) and firmware (04) HID
interfaces to the active desktop seat. An older settings-only rule can prevent
the EPOS web updater from seeing interface 04 and produce a misleading
“multiple devices connected” error; see the [captured attempt](tests/fixtures/firmware/WEB_ATTEMPT.md).

`--help-all` also works headlessly. JSON commands cannot be combined with each other or GUI options. Exit codes are 0 for success, 1 for output/startup failure, 2 for invalid arguments or screenshot-save failure, and 3 for an unavailable discovery backend. Diagnostics retain schema version 1 with additive `scanStatus`, `stale`, `capturedAt`, connection IDs, and structured `issues` fields. Missing/unreadable sysfs is distinct from a successful scan finding no headset.

The GUI retains previous data marked stale if enumeration fails. A stale selected device is not reported as connected. HID layouts are cached by connection and descriptor identity; permission probes remain live. Opening/refreshing diagnostics explicitly re-inspects metadata. udev events trigger coalesced refreshes; the fallback interval is 30 seconds with a working monitor and 1.5 seconds otherwise. Discovery never blocks the GUI thread with filesystem access.

## Optional SDK wrapper

`scripts/with-sdk.sh` supports an already provisioned SDK with this layout under `$HOME/.local/share/epos-controller` (or `EPOS_SDK_ROOT`):

```text
Qt/6.12.0/gcc_64/
cmake-4.4.4-linux-x86_64/
```

It selects those tools and clears inherited Qt/library path overrides. It does not install the SDK and is not required for a normal system/Qt SDK build:

```sh
bash scripts/with-sdk.sh cmake --preset release
bash scripts/with-sdk.sh cmake --build --preset release --parallel
bash scripts/with-sdk.sh ctest --preset release
```

Use `cmake --fresh --preset release` when switching an existing build to a different Qt SDK. `EPOS_EXTRA_PREFIX_PATH` accepts additional colon-separated dependency prefixes while keeping the selected Qt SDK first.

## Installation and packaging

The default install uses Qt's deployment API to stage matching shared libraries, QML modules, desktop platform plugins, and a relative `qt.conf`; it no longer relies on whichever Qt happens to be installed on the host. The desktop entry, icon, project license, and dependency notice are included.

```sh
cmake --install build/release --prefix "$PWD/build/stage"
./build/stage/bin/headset-controller --diagnostics
python3 scripts/check-install.py build/release
```

The install check stages to a temporary directory, moves the installation, removes inherited Qt/loader path overrides, verifies Qt library resolution, and exercises every page plus diagnostics. It checks relocation on the build host, not every Linux distribution. System libc, libstdc++, libudev, graphics drivers, and native desktop dependencies must remain compatible. The staged application also passed all-page offscreen startup and diagnostics in the clean Ubuntu 26.04 x86-64 runtime defined by `packaging/smoke.Dockerfile`, without system Qt. Native display-driver interaction still needs target-machine validation. `cpack --config build/release/CPackConfig.cmake` produces a TGZ staging archive. Complete the SDK-specific licensing/source-material requirements in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) before distributing it.

Distro packagers can use `-DHEADSET_DEPLOY_QT=OFF` and declare matching Qt >= 6.12 and other native dependencies in their package metadata. This mode intentionally does not bundle Qt and is not suitable for an SDK-only machine with an older system Qt.

## Parser-only builds, fuzzing, and benchmarks

```sh
cmake --preset parser
cmake --build --preset parser
ctest --preset parser
```

Parser-only builds require C++23 `std::expected` but neither Qt, libudev, nor `std::move_only_function`. For Clang with a compatible standard library, configure `-DHEADSET_PARSER_ONLY=ON -DHEADSET_FUZZING=ON`, then seed and run the fuzzer:

```sh
python3 scripts/seed-fuzz.py build/corpus
./build/fuzz/hid-descriptor-fuzz build/corpus -max_total_time=30 -max_len=8193
```

Configure a normal build with `-DHEADSET_BENCHMARKS=ON` to build `discovery-benchmark`. It measures warm fixture scans for one headset with 0/10/50 unrelated devices and checks that results remain identical. See [CODEBASE_REVIEW.md](CODEBASE_REVIEW.md) for the original measurements and finding dispositions.

## Source layout

- `app/`: discovery, diagnostics, presentation adapters and application startup.
- `app/settings/`: typed settings domain, protocol codec, transaction service and Linux report transport.
- `app/firmware/`: firmware identity, release/package validation, update protocol, Linux transport and persistent operation records.
- `qml/`: interface pages and shared controls.
- `tests/`: backend and parser regression tests.
- `cmake/`: compiler hardening and sanitizer options.
- `scripts/`: optional development tooling.
- `packaging/`: desktop integration assets.
- [CONTRIBUTING.md](CONTRIBUTING.md): tracked engineering, testing, and translation workflow.
