# Linux binary release packaging

The 1.0.0 binary is built from the unchanged release tag on Ubuntu 26.04 x86-64
with GCC 15.2, CMake 4.4.4 and the official Qt 6.12.0 Linux SDK. Packaging files
and user documentation may be newer than the tag; application code must match it.
Do not retag a published release to add assets.

## Build and stage

Use a fresh checkout or the versioned source archive and a separate Release
build. `BUILD_TESTING=OFF` is appropriate after the tagged code's tests pass.

```sh
cmake -S /path/to/v1.0.0 -B /path/to/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.12.0/gcc_64
cmake --build /path/to/build --parallel
```

Use `scripts/package-linux-release.py` from the current packaging checkout:

```sh
python3 scripts/package-linux-release.py \
  --build /path/to/build --qt /path/to/Qt/6.12.0/gcc_64 \
  --output /path/to/new-output-directory
```

The script stages the application, checks its version, downloads checksum-pinned
Qt sources and ICU notices, preserves Qt SDK build configurations/SBOMs, and
creates binary and corresponding-source archives plus `SHA256SUMS.binary`.
It requires Python 3.11+, CMake, `ldd`, and HTTPS access. Its output directory
must not already exist. It packages this release's dependency versions explicitly;
update the pinned source hashes and review notices for later SDK versions.

Only Qt and ICU are bundled. Native system libraries copied by Qt deployment
(libarchive and OpenGL dispatch libraries) are removed from the staging tree and
declared as distribution dependencies. Unexpected bundled libraries stop packaging.
All Qt source archives are retained intact, including their build scripts and
embedded dependency sources. The separate source asset avoids inflating the
application download. Qt libraries remain replaceable shared objects.

## Verify the actual archive

Extract the binary archive into a different path. Check `--version`, JSON
diagnostics, `--smoke-test --page N` for all four pages, and
`--smoke-test --open-diagnostics`. Clear inherited Qt and loader path overrides.
Check the helper's `--print-rule` against the packaged rule; never run its
privileged install mode in release tests.

Build `packaging/smoke.Dockerfile` and mount the relocated bundle at `/opt/headset`
read-only. Repeat the smoke checks with `--network none`. The image has native
Ubuntu dependencies but no Qt SDK or system Qt. Also check X11 and Wayland
plugins' dependencies with `ldd`; a headless test does not validate native
display-server behavior, audio delivery or physical device transactions.

## Publish

Upload the binary and Qt source archives to the existing `v1.0.0` release.
Merge their checksums into the release's existing `SHA256SUMS`, preserving the
original application-source checksum. Update the release body from
`releases/1.0.0.md`. Download the published assets and verify their checksums.

Do not commit generated archives, downloaded dependency sources or SDK files.
