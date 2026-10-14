"""Hermetic offline inspection tests using synthetic containers, never real firmware/HID."""
import copy
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import warnings
import zipfile

binary = str(Path(sys.argv[1]).resolve())
env = dict(os.environ, DISPLAY="", WAYLAND_DISPLAY="", QT_QPA_PLATFORM="invalid")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def record(tag, payload):
    return tag + struct.pack(">I", len(payload)) + payload


header = record(b"APPUHDR5", b"QCC518Xx" + bytes(28))
partition = record(b"PARTDATA", struct.pack(">I", 7) + b"synthetic-test-image")
footer = record(b"APPUPFTR", bytes(256))
image = header + partition + footer
xml = b"""<?xml version="1.0"?><SeComAppHSFWU>
<Model>IMPACT 860T ANC</Model><FWVersion><Version>1.5.619</Version></FWVersion>
<FWFiles><FWFile>image.bin</FWFile></FWFiles><ReleaseNote>notes.txt</ReleaseNote>
</SeComAppHSFWU>"""
original = [("image.bin", image), ("PackageInfo.xml", xml), ("notes.txt", b"Synthetic test fixture")]
url = "https://update.eposstorage.com/deviceupdates/test.zip"


def archive(entries):
    buffer = io.BytesIO()
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", UserWarning)
        with zipfile.ZipFile(buffer, "w", compression=zipfile.ZIP_DEFLATED) as z:
            for name, content in entries:
                z.writestr(name, content)
    return buffer.getvalue()


def offer(data, entries):
    return {"Type": "Firmware", "IProduct": "IMPACT_860T_ANC", "Version": "1.5.619",
            "UpdatesURL": url, "UpdatesURLs": [{"UpdateVariant": "FW", "IsDefault": True,
            "UpdatesURL": url, "Hash": digest(data),
            "Files": [{"Name": n.filename if isinstance(n, zipfile.ZipInfo) else n, "Hash": digest(b)}
                      for n, b in entries]}]}


with tempfile.TemporaryDirectory(prefix="headset-firmware-") as directory:
    root = Path(directory)
    package = root / "package.zip"
    catalog = root / "offer.json"

    def run(entries=original, mutate=None, installed="1.5.64", success=False, zip_bytes=None,
            expected_code=None):
        data = archive(entries) if zip_bytes is None else zip_bytes
        metadata = offer(data, entries)
        if mutate:
            mutate(metadata)
        package.write_bytes(data)
        catalog.write_text(json.dumps(metadata))
        command = [binary, "--inspect-firmware", str(package), "--firmware-offer", str(catalog)]
        if installed is not None:
            command += ["--installed-version", installed]
        result = subprocess.run(command, env=env, text=True, capture_output=True, timeout=15)
        assert result.returncode == (expected_code if expected_code is not None else 0 if success else 4), result
        if expected_code == 2:
            return
        report = json.loads(result.stdout)
        assert report["installationSupported"] is False
        if success:
            assert report["publisherSignatureVerified"] is False
            assert report["deviceCompatibilityVerified"] is False
            assert report["liveCatalogChecked"] is False
            assert report["packageSha256"] == digest(data)
            assert report["imageBytes"] == len(image)
            assert len(report["imageRecords"]) == 3
        else:
            assert report["error"], report
        return report

    assert run(success=True)["versionComparison"] == "newer"
    assert run(success=True, installed="1.5.619")["versionComparison"] == "same"
    assert run(success=True, installed="1.6.0")["versionComparison"] == "older"
    assert "versionComparison" not in run(success=True, installed=None)
    for version in ["1.5", "1.5.619.0", "-1.5.619", "1.5.65536", "1.x.2", " 1.5.64", "1.5.64x"]:
        run(installed=version, expected_code=2)
    for key, value in [("IProduct", "OTHER_MODEL"), ("Type", "Software"), ("Version", "1.5.0"),
                       ("Version", "1.5.99999"), ("UpdatesURLs", [])]:
        run(mutate=lambda m, k=key, v=value: m.update({k: v}))
    for key, value in [("IsDefault", "true"), ("Hash", "0" * 64), ("Hash", "z" * 64),
                       ("UpdatesURL", "http://update.eposstorage.com/deviceupdates/test.zip"),
                       ("UpdatesURL", url + "?token=not-a-real-secret")]:
        run(mutate=lambda m, k=key, v=value: m["UpdatesURLs"][0].update({k: v}))
    run(mutate=lambda m: m["UpdatesURLs"].append(copy.deepcopy(m["UpdatesURLs"][0])))
    run(mutate=lambda m: m["UpdatesURLs"][0]["Files"][0].update(Hash="0" * 64))
    for bad in [b"", image[:-1], image + b"trailing", header + partition + partition + footer,
                record(b"APPUHDR5", b"wrong-header"), header + partition,
                b"APPUHDR5" + struct.pack(">I", 0xffffffff) + bytes(36),
                header + record(b"UNKNOWN!", bytes(8)) + footer]:
        run(entries=[("image.bin", bad), *original[1:]])
    for bad in [xml.replace(b"860T", b"1060T"), xml.replace(b"1.5.619", b"1.5.64"),
                xml.replace(b"image.bin", b"missing.bin"), xml.replace(b"image.bin", b"../image.bin"),
                xml.replace(b"</FWFiles>", b"<FWFile>image.bin</FWFile></FWFiles>"),
                xml.replace(b"<Model>", b'<Model extra="value">'), xml + b"<extra/>",
                b'<!DOCTYPE SeComAppHSFWU [<!ENTITY x "something">]>' + xml.split(b"?>", 1)[1]]:
        run(entries=[original[0], ("PackageInfo.xml", bad), original[2]])
    run(entries=original + [("extra.txt", b"Unexpected")])
    run(entries=original + [original[0]], mutate=lambda m: m["UpdatesURLs"][0]["Files"].pop())
    run(entries=[("../image.bin", image), *original[1:]])
    run(entries=[("image.bin", image), ("PackageInfo.xml", xml), ("IMAGE.BIN", b"Collision")])
    link = zipfile.ZipInfo("image.bin")
    link.create_system = 3
    link.external_attr = 0o120777 << 16
    run(entries=[(link, b"elsewhere"), *original[1:]])
    run(zip_bytes=archive(original)[:100])
    run(entries=[original[0], original[1], ("notes.txt", b"a" * (64 * 1024 + 1))])
    # The file reader must reject FIFOs instead of blocking on untrusted local paths.
    package.unlink()
    os.mkfifo(package)
    result = subprocess.run([binary, "--inspect-firmware", str(package), "--firmware-offer", str(catalog)],
                            env=env, text=True, capture_output=True, timeout=5)
    assert result.returncode == 4 and "regular file" in result.stdout
print("Offline firmware hashes, ZIP boundaries, manifest, version, container and no-install claims passed")
