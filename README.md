# Headset Controller for Linux

An independent C++23 / Qt Quick application for the EPOS IMPACT 860T ANC.

## Status

The application is currently a **read-only prototype**:

- Live USB discovery and disconnect/reconnect polling through Linux sysfs.
- HID interface inspection and JSON diagnostics, available in the GUI or without a display server.
- Home, Updates, Settings and Help screens, with unavailable settings clearly indicated.

Device settings and firmware updates are planned but not implemented. No device commands are sent, and no settings or firmware are changed. Firmware is reported as unknown rather than inferred from the USB revision. Only the first matching headset (`1395:018e`) is displayed.

This is an independent project, not an official EPOS application. No proprietary EPOS SDK is required, linked or included.

## Requirements

- Linux with sysfs.
- A C++23 compiler and standard library supporting `std::expected` and `std::move_only_function`; GCC/libstdc++ 15.2 is tested.
- CMake 3.24 or newer and Ninja.
- Qt 6.8 or newer with Core, Gui, Qml, Quick and QuickControls2 development components, plus QtQuick Controls, Layouts, Window and WorkerScript runtime modules.
- Native graphics development dependencies, including OpenGL/EGL and the dependencies required by your Qt installation.

The tested toolchain is Qt 6.12.0, CMake 4.4.4, Ninja 1.13.2 and GCC 15.2. The declared Qt 6.8 minimum has not yet been verified with a compatibility build. Distribution packages may be older than these requirements; install a coherent Qt SDK and a suitable compiler if needed.

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

Tests include discovery, descriptor parsing, malformed input and resource limits, headless CLI checks and offscreen GUI startup. The `dev` preset provides a Debug build. For AddressSanitizer and UndefinedBehaviorSanitizer checks with GCC/Clang:

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

Use `cmake --fresh --preset release` when switching an existing build to a different Qt SDK. Build outputs require their Qt runtime libraries; they are not standalone application bundles.

## Source layout

- `app/`: discovery, typed HID parsing, diagnostics and application startup.
- `qml/`: interface pages and shared controls.
- `tests/`: backend and parser regression tests.
- `cmake/`: compiler hardening and sanitizer options.
- `scripts/`: optional development tooling.
