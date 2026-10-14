# Contributor guide

This repository's tracked README, CMake presets, CI workflow, and this guide are
the authoritative build/contribution instructions. Ignored local research and
vendor archives are not required to build or test a checkout.

## Boundaries

- `HidDescriptor` is a portable, bounded layout inspector, not a complete HID
  validator or authorization to send commands. Preserve its typed errors and
  independent expected fixtures.
- `SysfsDeviceScanner` and `HidInspection` run on one owned discovery worker.
  Keep byte budgets, structured failures, cache invalidation, and deterministic
  ordering. Never call a scanner concurrently.
- `DeviceDiscovery` publishes value snapshots on its QObject/GUI thread. Refresh
  requests coalesce to at most one pending scan. Forced/topology refreshes
  invalidate older in-flight results. Shutdown joins the current scan before
  destroying the provider; future blocking transports need their own bounded
  timeouts/cancellation, not this local-filesystem contract.
- Connection IDs are **process-local generations**, not persistent device IDs.
  Adding an earlier-sorted device must not change selection. A removed/replaced
  selection stays disconnected until the user chooses a current connection.
  Future settings/updater requests must capture the selected generation and
  reject replies after removal/reselection; they must also reject stale state.
- Keep QML registration and clipboard use in `headset-ui`, not `headset-core`.
  Domain device/interface/error types are adapted to variants only for JSON and
  the selection list. Pages consume the typed `SelectedDevice` view.
- Unsupported settings in `SettingsCatalog` are inventory, not capabilities,
  defaults, or device readback. `app/settings/` owns settings values, constraints
  and transactions; `SettingsController` adapts them for QML. Keep Windows evidence
  in independent protocol fixtures, not in runtime architecture or dependencies.
  Firmware updates need a separate state machine. See [SETTINGS.md](SETTINGS.md).
- Settings reads run automatically while the GUI device-settings view is active,
  on page activation and changed selection; writes require explicit interaction.
  Keep screenshot/smoke modes descriptor-only. Keep one operation per worker,
  generation invalidation, bounded/cancellable input and fresh readback around
  writes. Never retry an ambiguous setter automatically. Kernel output ioctl
  cancellation is limited by its USB timeout; do not move it onto the GUI thread.
- CTest must not send commands to real hardware. The manual
  `headset-settings-probe` target requires explicit operator coordination and is
  excluded from normal builds, installation and CTest.

## Checks

Run the Release and sanitizer presets, `headset-controller_qmllint`, and
`scripts/check-install.py` for runtime/deployment changes. QML warnings fail lint
via `.qmllint.ini`. A parser-only build and optional Clang libFuzzer target provide
independent backend verification. Keep sanitizer findings unsuppressed unless a
specific external defect is documented.

Prefer behavior tests to implementation mirrors. Use checked fixture helpers,
isolated filesystem roots, explicit worker synchronization, and named Qt Test
assertions. UI tests must inject data instead of requiring physical hardware.
Use signal/property readiness rather than arbitrary sleeps. Keep a separate live
startup smoke test; physical unplug/replug and native desktop checks complement,
rather than replace, deterministic tests.

Normal GUI launches may automatically request system authorization to install
persistent headset access. Use `--smoke-test` or screenshot mode for unattended
checks; these never start access setup. `device-access-test` substitutes an
unprivileged child process and installs fixtures only in temporary directories.
Do not exercise the privileged helper's install mode in automated host tests.

Keep C++ formatting aligned with `.clang-format` using clang-format 21 and text
files with `.editorconfig`. Run `python3 scripts/check-format.py` (`CLANG_FORMAT`
can select a versioned executable). Make large mechanical formatting changes
separately from future behavioral edits. Include a short user-facing `CHANGELOG.md` entry when behavior
changes. Measure before replacing the parser's small map, introducing allocators,
or enabling IPO. The maintained benchmark and syscall counts are useful for
discovery regressions; results are host/cache dependent.

## Translation preparation

QML display strings use `qsTr()` and backend status messages use `tr()`; device
names and diagnostic codes remain literal data. The application currently ships
English only, with the application-language setting explicitly unavailable.
To start a real translation, use the matching Qt Linguist `lupdate` over `app/`
and `qml/`, review the resulting TS catalog, compile it with `lrelease`, and add
locale selection/loading with `QTranslator` plus translated UI tests. Do not
advertise a locale merely because source strings are marked for translation.

## Release boundary

The Qt minimum is 6.12, the verified baseline. Default installation deploys Qt
using relative runtime paths; system libraries/drivers still define platform
compatibility. Validate an installed archive in a clean supported distribution
and include the SDK's applicable source/license notices before publication. CI's
relocation test deliberately detects accidental fallback to host Qt.
