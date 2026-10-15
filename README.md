# Headset Controller for Linux

Control your **EPOS IMPACT 860T ANC** headset from Linux. Adjust audio and call
settings, receive mute notifications, and check for firmware updates in a native
Qt application.

Headset Controller is an independent, MIT-licensed project. It is not affiliated
with EPOS and does not require the proprietary EPOS SDK.

## Download

**[Download version 1.0.0](https://github.com/magnusoverli/epos_controller/releases/latest)**

Choose `headset-controller-1.0.0-linux-x86_64.tar.gz` for **Ubuntu 26.04 x86-64**.
The archive includes the application and its Qt runtime; no compiler or Qt SDK
is needed. Other distributions and older Ubuntu versions are not validated for
this binary. Source archives are available for building on other Linux systems.

### Install and launch

Download the binary archive and `SHA256SUMS` from the release page. In your
download directory:

```sh
sha256sum --check --ignore-missing SHA256SUMS
tar -xzf headset-controller-1.0.0-linux-x86_64.tar.gz
cd headset-controller-1.0.0-linux-x86_64
./bin/headset-controller
```

Keep the extracted directory together: the executable uses the libraries and
plugins alongside it. You can move the whole directory to a permanent location.
See the included `INSTALL.md` for native runtime dependencies and desktop-menu
setup. To uninstall, remove the extracted directory and any desktop shortcut
you created.

Run the app as your normal desktop user. When needed, a one-time administrator
prompt sets up persistent headset access. Subsequent launches and replugging on
the same USB port do not normally require another prompt.

## Features

- **Audio controls:** sidetone, ANC, adaptive ANC, ANC level, TalkThrough and audio
  limiting.
- **Call and headset preferences:** boom-arm and wear actions, Auto Pause,
  Smart Pause, voice prompts and language, ringtone, Busylight and buttons.
- **Mute notifications:** visual desktop alerts or visual alerts with an audible
  cue through the headset, with a configurable notification interval.
- **Firmware updates:** installed-version display, automatic checks for official
  releases and user-initiated installation.
- **Device recovery:** automatic discovery and same-headset reconnection after
  reset or unplug/replug.
- **Diagnostics:** USB/HID information and JSON export for troubleshooting.

## Using the app

Connect the headset and open **Settings**. Click a section header to expand it.
Dropdowns save immediately; sidetone saves when you release the slider. Changes
are verified in the background, and errors appear beside the affected setting.
There is no separate Apply step. **Refresh** reloads the device's current values.

Under **Sounds & Notifications**, choose **Visual only** or **Visual and audio**
to enable mute alerts. Keep the app running to receive them. The interval limits
how frequently speaking while muted can trigger an alert; it is not a periodic
reminder. Audio alerts require `pulseaudio-utils` (`paplay` and `pactl`), including
on PipeWire-Pulse desktops, and respect the headset's output volume and mute state.

Open **Updates** to see installed and available firmware. When an update is
available, choose **Install update** and confirm. Close other headset controllers
first, and keep the headset connected and the app open until installation ends.
Reinstalls and downgrades are not supported.

## Compatibility and limitations

- Supported model: **EPOS IMPACT 860T ANC**, USB ID `1395:018e`.
- All 19 device settings can be read. Core sidetone and ANC changes have hardware
  validation; several additional settings and Factory Reset still need broader
  hardware validation. See the [settings support details](SETTINGS.md).
- **Native firmware installation has not yet been hardware-tested.** Its protocol
  is tested against a captured successful vendor update. Interrupted updates
  have no automatic resume or rollback and may require EPOS recovery/support.
- Application preferences are not yet implemented. Full EPOS Connect feature
  parity and support for other headset models are not claimed.

See the [release notes](releases/1.0.0.md) for version-specific details.

## Troubleshooting

Use **Help → Diagnostics** to inspect the connected device and export its state.
The following commands also work without a display server:

```sh
./bin/headset-controller --list-devices
./bin/headset-controller --diagnostics
./bin/headset-controller --read-settings
./bin/headset-controller --read-firmware
./bin/headset-controller --help
```

If access setup fails or was cancelled, use **Retry access setup**. Automatic
setup requires PolicyKit and a desktop authentication agent. For manual or
headless setup, see [Linux device access](SETTINGS.md#linux-access).

[Report an issue](https://github.com/magnusoverli/epos_controller/issues) with your
Linux distribution, app version, headset model and steps to reproduce the problem.
Review diagnostics for device identifiers and local paths before sharing them.

## Build from source

Requirements: a C++23 compiler and standard library with `std::expected` and
`std::move_only_function`, CMake 3.24+, Ninja, Qt 6.12+ (Core, Gui, Qml, Quick,
QuickControls2, Network and DBus), libudev, libarchive and pkg-config. Default
test-enabled builds also require Qt Test, Python 3 and `dbus-run-session`.
Install your platform's OpenGL/EGL and other Qt development dependencies.

```sh
git clone --branch v1.0.0 --depth 1 https://github.com/magnusoverli/epos_controller.git
cd epos_controller
cmake --preset release
cmake --build --preset release --parallel
./build/release/headset-controller
```

For a non-system Qt SDK, add `-DCMAKE_PREFIX_PATH=/path/to/Qt` to the configure
command. Use `-DBUILD_TESTING=OFF` for an application-only build.

See [CONTRIBUTING.md](CONTRIBUTING.md) for development and testing, and
[binary packaging](packaging/BINARY_RELEASE.md) for release-bundle instructions.

## License

Project source is licensed under [MIT](LICENSE). Bundled dependencies retain
their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md). EPOS and
IMPACT are trademarks of their respective owners.
