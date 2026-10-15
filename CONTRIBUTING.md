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

### Build environments

The release baseline is Qt 6.12.0, CMake 4.4.4 and GCC 15.2 on Ubuntu 26.04
x86-64. CI additionally builds with GCC 14, checks the parser with Clang/libc++,
and fuzzes with Clang/libstdc++. Application builds require C++23 `std::expected`
and `std::move_only_function`; parser-only builds require only `std::expected`.

For an already provisioned local SDK, `scripts/with-sdk.sh` selects
`$EPOS_SDK_ROOT` (default `$HOME/.local/share/epos-controller`) containing
`Qt/6.12.0/gcc_64/` and `cmake-4.4.4-linux-x86_64/`. It clears inherited Qt and
loader overrides but does not install dependencies:

```sh
bash scripts/with-sdk.sh cmake --preset release
bash scripts/with-sdk.sh cmake --build --preset release --parallel
bash scripts/with-sdk.sh ctest --preset release
```

Use `cmake --fresh --preset release` when changing SDKs. Additional dependency
prefixes can be supplied through `EPOS_EXTRA_PREFIX_PATH`.

### Validation commands

```sh
ctest --preset release --output-on-failure
cmake --build build/release --target headset-controller_qmllint
cmake --preset sanitize
cmake --build --preset sanitize --parallel
ctest --preset sanitize --output-on-failure
python3 scripts/check-install.py build/release
```

The install check stages and relocates the app, checks Qt linkage with inherited
loader overrides removed, and starts every page without device transactions.
See [binary packaging](packaging/BINARY_RELEASE.md) for distribution-archive and
clean-container checks. For distro packages using a compatible system Qt,
configure `-DHEADSET_DEPLOY_QT=OFF` and declare the runtime dependencies explicitly.

The Qt-independent parser has its own preset:

```sh
cmake --preset parser
cmake --build --preset parser
ctest --preset parser
```

For libFuzzer, configure with Clang and `-DHEADSET_PARSER_ONLY=ON
-DHEADSET_FUZZING=ON`. Match the C++ runtime used to build the installed libFuzzer
(Ubuntu's package uses libstdc++). Seed with
`python3 scripts/seed-fuzz.py build/corpus`, then run
`build/fuzz/hid-descriptor-fuzz build/corpus -max_total_time=30 -max_len=8193`.
`-DHEADSET_BENCHMARKS=ON` adds the `discovery-benchmark` target.

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
