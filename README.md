# Headset Controller for Linux

An independent C++23 / Qt Quick application for the EPOS IMPACT 860T ANC.

## Status

The application is currently a **read-only prototype**:

- Asynchronous USB discovery through Linux sysfs, with debounced udev hotplug events and fallback polling.
- HID interface inspection and JSON diagnostics, available in the GUI or without a display server.
- Home, Updates, Settings and Help screens, with unavailable settings clearly indicated.

Device settings and firmware updates are planned but not implemented. No device commands are sent, and no settings or firmware are changed. Firmware is reported as unknown rather than inferred from the USB revision. Supported hardware is `1395:018e`. The first discovered headset is initially selected; the selector allows choosing another. After removal/replacement, select the new connection explicitly. Selection never silently follows a different physical connection at the same USB port.

The project-owned source is MIT licensed; see [LICENSE](LICENSE) and [third-party notices](THIRD_PARTY_NOTICES.md).

This is an independent project, not an official EPOS application. No proprietary EPOS SDK is required, linked or included.

## Requirements

- Linux with sysfs.
- A C++23 compiler and standard library supporting `std::expected` and `std::move_only_function`; GCC/libstdc++ 15.2 is tested.
- CMake 3.24 or newer and Ninja.
- Qt 6.12 or newer with Core, Gui, Qml, Quick and QuickControls2 development components, plus QtQuick Controls, Layouts, Window and WorkerScript runtime modules.
- libudev development files and pkg-config.
- Qt Test and Python 3 for the default test-enabled builds (use `-DBUILD_TESTING=OFF` for an application-only build).
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

Run the application as your normal user. Current read-only sysfs discovery does not require root or hidraw access. Building does not download dependencies or install device-access rules. A clean-machine build remains to be validated; the commands above assume the dependencies are already installed.

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

### Command-line diagnostics

```sh
./build/release/headset-controller --list-devices
./build/release/headset-controller --diagnostics
./build/release/headset-controller --help
```

`--list-devices` and `--diagnostics` work without a display server. Diagnostics may contain device identifiers and local paths; review them before sharing.

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

- `app/`: discovery, typed HID parsing, diagnostics and application startup.
- `qml/`: interface pages and shared controls.
- `tests/`: backend and parser regression tests.
- `cmake/`: compiler hardening and sanitizer options.
- `scripts/`: optional development tooling.
- `packaging/`: desktop integration assets.
- [CONTRIBUTING.md](CONTRIBUTING.md): tracked engineering, testing, and translation workflow.
