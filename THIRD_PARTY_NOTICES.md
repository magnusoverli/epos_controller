# Third-party notices

The MIT license in `LICENSE` covers project-owned source, not its dependencies.
EPOS and IMPACT are trademarks of their respective owners. This is an independent
application. No EPOS SDK, firmware, artwork, or archived vendor documentation is
linked into or installed with the application.

## Qt

The application dynamically links Qt Core, Gui, Qml, Quick and Quick Controls,
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

The default CMake install deploys matching shared libraries using Qt's deployment
API. It is a relocatable development staging layout; a public release must also
include its SDK's licensing material and corresponding-source distribution.
Do not infer that a runtime smoke test establishes these distribution obligations.

## Linux runtime

The backend uses libudev from systemd (LGPL-2.1-or-later); source and notices:
https://github.com/systemd/systemd. The C++ runtime, C library, display drivers,
and libraries resolved from the target distribution retain their own licenses.
Qt's deployment API may copy additional non-system runtime dependencies from the
SDK (for example ICU); review the staged library inventory against that SDK's
third-party notices before publishing a binary archive.

Qt Test and libFuzzer are development-only dependencies and are not installed.
