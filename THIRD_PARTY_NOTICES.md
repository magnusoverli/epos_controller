# Third-party notices

The MIT license in `LICENSE` covers project-owned source, not its dependencies.
EPOS and IMPACT are trademarks of their respective owners. This is an independent
application. No EPOS SDK, firmware, artwork, or archived vendor documentation is
linked into or installed with the application.

## Qt

The application dynamically links Qt Core, Gui, Qml, Quick, Quick Controls,
Network and DBus,
including their runtime plugins and QML imports. Qt is copyright The Qt Company
Ltd. and other contributors. The supported SDK baseline is Qt 6.12.

Qt's open-source distribution offers LGPL-3.0/GPL terms for these modules;
commercial Qt installations have separate terms. A local SDK installation does
not determine redistribution rights by itself. Use an appropriately licensed SDK.
Open-source bundle distributors must include the applicable license texts and
module/third-party notices, retain the ability to replace the shared libraries,
and provide the corresponding Qt source under its applicable terms.

- Terms: https://www.qt.io/licensing/open-source-lgpl-obligations
- LGPLv3: https://www.gnu.org/licenses/lgpl-3.0.html
- GPLv3 (incorporated by LGPLv3): https://www.gnu.org/licenses/gpl-3.0.html
- Exact-release source archives: https://download.qt.io/official_releases/qt/
- Module and embedded dependency notices: https://doc.qt.io/qt-6/licenses.html

The published Ubuntu 26.04 x86-64 archive includes Qt 6.12.0 shared libraries,
plugins and QML modules. License texts, embedded third-party notices, SDK
configuration and binary SBOMs are in `share/headset-controller/licenses/`.
The `headset-controller-1.0.0-qt-sources.tar.gz` asset on the same release page
provides the corresponding unmodified Qt module sources and build instructions.
Qt libraries remain replaceable; reverse engineering for debugging modifications
to them is not prohibited by this application.

The raw CMake install is a development staging layout. For redistributable
archives, use the [binary packaging workflow](https://github.com/magnusoverli/epos_controller/blob/main/packaging/BINARY_RELEASE.md).

## ICU

The binary archive includes ICU 73.2 shared libraries from the Qt SDK.
Copyright © 1991–2023 Unicode, Inc. and other contributors. The Unicode license
and embedded third-party notices are included in
`share/headset-controller/licenses/ICU-73.2-LICENSE`.
Source: https://github.com/unicode-org/icu/releases/tag/release-73-2.

## libarchive

Offline firmware ZIP inspection dynamically links libarchive. It is distributed
under BSD-style terms; its source, per-file notices and dependency details are at
https://github.com/libarchive/libarchive (see `COPYING`). Binary distributors must
retain the applicable notices for the version and compression libraries shipped.
The published binary archive uses the distribution's libarchive package rather
than bundling it. Inspected vendor firmware stays outside the application distribution.

## Linux runtime

The backend uses libudev from systemd (LGPL-2.1-or-later); source and notices:
https://github.com/systemd/systemd. The C++ runtime, C library, display drivers,
and libraries resolved from the target distribution retain their own licenses.
Qt's deployment API may copy additional non-system runtime dependencies from the
SDK (for example ICU); review the staged library inventory against that SDK's
third-party notices before publishing a binary archive.

Qt Test and libFuzzer are development-only dependencies and are not installed.
