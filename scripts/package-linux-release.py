#!/usr/bin/env python3
"""Package the tagged 1.0.0 Linux build with Qt sources and dependency notices."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tarfile
import urllib.request

VERSION = "1.0.0"
REVISION = "19a17dc45f0839368779a4d312b7271fadfbd1b6"
QT_VERSION = "6.12.0"
QT_SOURCES = {
    "qtbase": "a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307",
    "qtdeclarative": "311f3a2603e1973bb59baef9dfa740a376de713157d4782ee043681e889c9260",
    "qtshadertools": "c7d84f436e1aaef39fdcadebf2bd71bdf24dc69497e13f2e0198873cbbfea2ab",
    "qtsvg": "e4ab39534ec97987b1b9b60ef7f4d3253d5912a69f8c713a01753174a4029331",
    "qttasktree": "d28cac0d1fc2d87fbc8fd70d4f956a79b2d37a039df2766e9ff0f69635596b5e",
    "qtwayland": "ae14d9bcf1bb3c300a3ab2e6a53f50284e5c6c966a4ac4cc5aedcb1bc74e4d02",
}
ICU_LICENSE_HASH = "f3005e195ff74d8812cc1f182a1c446fab678d70a10e3dada497585befee5416"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def download(url, path, expected, cache):
    cached = cache / path.name if cache else None
    if cached and cached.is_file() and digest(cached) == expected:
        shutil.copyfile(cached, path)
    else:
        with urllib.request.urlopen(url, timeout=120) as source, path.open("wb") as output:
            shutil.copyfileobj(source, output)
    if digest(path) != expected:
        raise RuntimeError(f"Checksum mismatch: {path.name}")


def copy_notices(archive, destination):
    # Copy regular notice files only: never extract paths or links from an archive.
    with tarfile.open(archive) as source:
        for member in source.getmembers():
            path = PurePosixPath(member.name)
            if not member.isfile() or path.is_absolute() or ".." in path.parts:
                continue
            if ("LICENSES" in path.parts or path.name == "qt_attribution.json"
                    or re.search(r"licen[cs]e|copying|copyright|notice|authors|readme|^ftl\.",
                                 path.name, re.IGNORECASE)):
                output = destination.joinpath(*path.parts[1:])
                output.parent.mkdir(parents=True, exist_ok=True)
                with source.extractfile(member) as stream, output.open("wb") as target:
                    shutil.copyfileobj(stream, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--qt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cache", type=Path, help="Optional checksum-verified download cache")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parent.parent
    cache_entries = {}
    for line in (args.build / "CMakeCache.txt").read_text().splitlines():
        if ":" in line and "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            cache_entries[key.split(":", 1)[0]] = value
    source_root = Path(cache_entries["CMAKE_HOME_DIRECTORY"])
    if cache_entries.get("CMAKE_BUILD_TYPE") != "Release":
        raise RuntimeError("A Release build is required")
    tracked = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", REVISION],
                                      cwd=repo, text=True).splitlines()
    for name in tracked:
        if name.startswith(("app/", "qml/", "cmake/", "packaging/")) or name == "CMakeLists.txt":
            expected = subprocess.check_output(["git", "show", f"{REVISION}:{name}"], cwd=repo)
            if (source_root / name).read_bytes() != expected:
                raise RuntimeError(f"Build source differs from v{VERSION}: {name}")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    stage = output / f"headset-controller-{VERSION}-linux-x86_64"
    sources = output / f"headset-controller-{VERSION}-qt-sources"
    sources.mkdir()
    subprocess.run(["cmake", "--install", str(args.build.resolve()), "--prefix", str(stage)], check=True)
    # Qt's deployer may copy these from an SDK sysroot. Use the distro packages.
    for pattern in ("libarchive.so*", "libGLX.so*", "libOpenGL.so*"):
        for path in (stage / "lib").glob(pattern):
            path.unlink()
    for path in (stage / "lib").iterdir():
        if not (path.name.startswith("libQt6") and ".so.6" in path.name
                or re.fullmatch(r"libicu(data|i18n|uc)\.so\.73(\.2)?", path.name)):
            raise RuntimeError(f"Unreviewed bundled library: {path.name}")
    env = os.environ.copy()
    for key in ("LD_LIBRARY_PATH", "QT_PLUGIN_PATH", "QML_IMPORT_PATH", "QML2_IMPORT_PATH",
                "QT_QPA_PLATFORM_PLUGIN_PATH"):
        env.pop(key, None)
    binary = stage / "bin/headset-controller"
    version = subprocess.check_output([binary, "--version"], env=env, text=True).strip()
    if version != f"Headset Controller {VERSION}":
        raise RuntimeError(f"Unexpected application version: {version}")
    if not (stage / "lib" / f"libQt6Core.so.{QT_VERSION}").is_file():
        raise RuntimeError("Unexpected deployed Qt version")
    doc = stage / "share/headset-controller"
    licenses = doc / "licenses"
    configs = sources / "sdk-build-config"
    configs.mkdir()
    checksums = []
    for module, expected in QT_SOURCES.items():
        name = f"{module}-everywhere-src-{QT_VERSION}.tar.xz"
        archive = sources / name
        url = f"https://download.qt.io/official_releases/qt/6.12/{QT_VERSION}/submodules/{name}"
        download(url, archive, expected, args.cache)
        checksums.append(f"{expected}  {name}\n")
        copy_notices(archive, licenses / module)
        for suffix in ("opt", "summary"):
            name = f"config_{module}.{suffix}"
            shutil.copyfile(args.qt / name, configs / name)
        for suffix in ("cdx.json", "spdx.json"):
            name = f"{module}-{QT_VERSION}.{suffix}"
            target = licenses / "sbom" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(args.qt / "sbom" / name, target)
    shutil.copytree(configs, licenses / "sdk-build-config")
    download("https://raw.githubusercontent.com/unicode-org/icu/release-73-2/icu4c/LICENSE",
             licenses / "ICU-73.2-LICENSE", ICU_LICENSE_HASH, args.cache)
    (sources / "SHA256SUMS").write_text("".join(checksums))
    shutil.copyfile(repo / "packaging/QT_SOURCES.md", sources / "README.md")
    shutil.copyfile(repo / "packaging/INSTALL-linux.md", stage / "INSTALL.md")
    readme = re.sub(r"\]\((?!https?://|#)([^)]+)\)",
                    r"](https://github.com/magnusoverli/epos_controller/blob/main/\1)",
                    (repo / "README.md").read_text())
    (stage / "README.md").write_text(readme)
    shutil.copyfile(repo / "THIRD_PARTY_NOTICES.md", doc / "THIRD_PARTY_NOTICES.md")
    # These are documentation/desktop metadata overlays, not changes to the executable.
    desktop = stage / "share/applications/headset-controller.desktop"
    desktop.write_text(desktop.read_text().replace("Read-only headset discovery and USB diagnostics",
                                                 "Headset settings, notifications and firmware updates"))
    manifest = {"version": VERSION, "sourceCommit": REVISION, "platform": "Ubuntu 26.04 x86-64",
                "qtVersion": QT_VERSION, "icuVersion": "73.2", "compiler": "GCC 15.2",
                "files": {str(p.relative_to(stage)): digest(p) for p in sorted(stage.rglob("*"))
                          if p.is_file() and not p.is_symlink()}}
    (stage / "BUILD-MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
    archive_checksums = []
    for directory in (stage, sources):
        archive = output / f"{directory.name}.tar.gz"
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(directory, arcname=directory.name)
        archive_checksums.append(f"{digest(archive)}  {archive.name}\n")
        print(f"Created {archive} ({archive.stat().st_size:,} bytes)")
    (output / "SHA256SUMS.binary").write_text("".join(archive_checksums))


if __name__ == "__main__":
    main()
