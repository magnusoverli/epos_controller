# Qt sources for Headset Controller 1.0.0

These unmodified official Qt 6.12.0 module archives correspond to the Qt
libraries/plugins in the Ubuntu 26.04 x86-64 binary release. They include full
source, build scripts, license texts and embedded third-party sources. Verify
them with `sha256sum --check SHA256SUMS`.

Upstream location:
https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/

Included modules: qtbase, qtdeclarative, qtsvg and qtwayland, plus build
dependencies qtshadertools and qttasktree. Original SDK configuration options
and summaries are in `sdk-build-config/`; the Qt binaries were supplied by the
official Linux GCC SDK. Qt's source and binary SBOM metadata identify embedded
dependencies; binary SBOMs accompany the application in its `licenses/sbom/`.

## Rebuilding and replacing Qt

Install the Linux development dependencies required by the modules and configure
an out-of-tree Qt build. For Qt Base, a typical shared-library build is:

```sh
tar -xf qtbase-everywhere-src-6.12.0.tar.xz
mkdir qtbase-build
cd qtbase-build
../qtbase-everywhere-src-6.12.0/configure -prefix /your/qt-prefix \
  -opensource -confirm-license -release -nomake tests -nomake examples
cmake --build . --parallel
cmake --install .
```

Extract and build the remaining modules with
`/your/qt-prefix/bin/qt-configure-module /path/to/extracted-module` in separate
build directories, followed by `cmake --build . --parallel` and `cmake --install .`.
Build qtsvg, qtshadertools and qttasktree before qtdeclarative, then qtwayland.
Consult the recorded SDK options to match the original feature configuration;
tool paths and build-cache/SBOM tooling paths are specific to Qt's build host and
should be adapted to your system.

Replace the shared objects in the application's `lib/`, together with matching
plugins and QML modules in `plugins/` and `qml/`. Preserve their sonames and keep
the Qt version and build configuration compatible across all modules. The
application loads libraries relative to its own install directory; it does not
validate their hashes at startup or prohibit replacement. Alternatively rebuild
the MIT-licensed application against your Qt installation.

ICU 73.2 is also bundled with the application. Its full license and embedded
third-party notices are in `licenses/ICU-73.2-LICENSE` in the binary archive.
ICU source is available at https://github.com/unicode-org/icu/releases/tag/release-73-2.
