#!/usr/bin/env bash
# Run a command with this project's persistent Linux SDK, without changing
# the login shell or replacing the desktop's Qt libraries.
set -euo pipefail

if [[ $# -eq 0 ]]; then
    printf 'Usage: bash scripts/with-sdk.sh COMMAND [ARGUMENTS...]\n' >&2
    exit 2
fi

sdk_root="${EPOS_SDK_ROOT:-$HOME/.local/share/epos-controller}"
qt_root="$sdk_root/Qt/6.12.0/gcc_64"
cmake_root="$sdk_root/cmake-4.4.4-linux-x86_64"

if [[ ! -x "$qt_root/bin/qmake" || ! -x "$cmake_root/bin/cmake" ]]; then
    printf 'SDK not found in %s. See the Optional SDK wrapper section in README.md.\n' "$sdk_root" >&2
    exit 1
fi

# Avoid inheriting the old extracted-toolchain loader/import overrides.
unset LD_LIBRARY_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
export PATH="$cmake_root/bin:$qt_root/bin:$PATH"
export CMAKE_PREFIX_PATH="$qt_root"
if [[ -d "$sdk_root/sysdeps/root/usr" ]]; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH:$sdk_root/sysdeps/root/usr"
fi
if [[ -n "${EPOS_EXTRA_PREFIX_PATH:-}" ]]; then
    export CMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH:$EPOS_EXTRA_PREFIX_PATH"
fi
exec "$@"
