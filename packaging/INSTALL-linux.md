# Headset Controller 1.0.0 — Linux x86-64

## Run

This build targets **Ubuntu 26.04 x86-64** and includes Qt 6.12.0 and ICU 73.2.
It requires the distribution's native desktop libraries, glibc and C++ runtime.
Older Ubuntu releases and other distributions are not validated.

Extract the archive, keep its contents together, and run:

```sh
./bin/headset-controller
```

Run as your normal user, not root. Connect an EPOS IMPACT 860T ANC headset.
If prompted, authorize the one-time persistent device-access setup.

## Native dependencies

On Ubuntu 26.04, most of these libraries are already installed with the desktop.
To install the runtime dependencies explicitly:

```sh
sudo apt-get install \
  libstdc++6 libudev1 libarchive13t64 libgl1 libopengl0 libegl1 \
  libfontconfig1 libxkbcommon0 libdbus-1-3 libglib2.0-0t64 \
  libgssapi-krb5-2 libssl3t64 ca-certificates \
  libxcb-cursor0 libxkbcommon-x11-0 libsm6 libice6 libxcb-icccm4 \
  libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 \
  libxcb-xfixes0 libxcb-sync1 libxcb-randr0 libxcb-xinerama0 \
  libxcb-xkb1 libxcb-glx0 libx11-xcb1 \
  libwayland-client0 libwayland-cursor0 libwayland-egl1 \
  pkexec pulseaudio-utils
```

Automatic access setup also needs a running PolicyKit desktop authentication
agent. Notifications use your desktop's D-Bus notification service. Audible
alerts use `paplay`/`pactl`, including on PipeWire-Pulse systems. Firmware
installation uses systemd-logind to inhibit sleep and shutdown.

## Optional desktop-menu shortcut

Move the extracted directory to a permanent location first. Copy
`share/applications/headset-controller.desktop` into
`~/.local/share/applications/` (create that directory if needed). Edit the copied
file so that:

- `Exec=` contains the absolute path to `bin/headset-controller`, quoted if the
  path contains spaces.
- `Icon=` contains the absolute path to
  `share/icons/hicolor/scalable/apps/headset-controller.svg`.

The application will then appear as **Headset Controller** in the desktop menu.
If you move the application directory later, update those paths.

## Updates and uninstalling

Application releases are available at
https://github.com/magnusoverli/epos_controller/releases.
The in-app Updates page updates **headset firmware**, not this application.

To uninstall, close the app, remove its extracted directory and delete any
desktop shortcut you created. Saved preferences, firmware interruption records
and the system device-access rule are retained. Do not delete interruption
records to bypass a failed firmware update; follow EPOS recovery/support guidance.

## Source and licenses

The executable is built from the `v1.0.0` tag, commit
`19a17dc45f0839368779a4d312b7271fadfbd1b6`. See the source archive on the same
release page and `BUILD-MANIFEST.json` for binary hashes and dependency versions.

Project license and dependency notices are in `share/headset-controller/`.
Qt license texts, embedded third-party notices and SDK metadata are under
`share/headset-controller/licenses/`. The full corresponding Qt source archives
and build configurations are supplied in
`headset-controller-1.0.0-qt-sources.tar.gz` on the same release page.

Qt is dynamically linked. You may modify or replace its libraries in `lib/`,
and the matching plugins/QML modules in `plugins/` and `qml/`, with compatible
builds. No application restriction prohibits reverse engineering for debugging
modifications to these libraries. See the included LGPLv3/GPLv3 license texts
and source-bundle instructions for rebuilding Qt.
