# Live production catalog — 2026-10-14

Installation follow-up: [successful Web capture and native implementation](WEB_SUCCESS.md).

## Discovery and observed responses

The former `https://eposconnect.enterprise.eposaudio.com/` redirects requests,
including asset paths, to the root of **https://connect.eposaudio.com/**.
Fetching scripts from the new host directly works. This explains why previous
research saw HTML instead of JavaScript.

Public assets were downloaded and inspected as bytes, not executed:

| Asset | SHA-256 |
|---|---|
| `main.dart.js` | `ffdc4d7024afb699dc2224c31081a0129aa4297503d682d5ea08d022178f909f` |
| `dci/dci-wasm.wasm` | `4d91c621fff3402874639d1978a75c90fada104f27eeb67d0555be98560a13f8` |
| `assets/assets/config/config.json` | `2239f60b1344c1d00f6bf06cd12ef5b47a1c737381757fe00c4649118cbbcd53` |

The public configuration declares API host `api.enterprise.eposaudio.com`,
public tenant `1e89135c-7e56-4ad3-821c-e278427918a0`, disabled user/device
registration and disabled SSO. This tenant is published configuration, not a
captured account credential. The WASM contains these string templates:

- File offset `0x1cf110`: `Updates/AvailableFirmware/{}?tenantId={}`
- File offset `0x1cf254`: `{}/api/{}`

Credential-free HTTPS GET, observed HTTP 200 with a JSON array:

```
https://api.enterprise.eposaudio.com/api/Updates/AvailableFirmware/IMPACT_860T_ANC?tenantId=1e89135c-7e56-4ad3-821c-e278427918a0
```

Response fields: `Version`, `FirmwareReceivedOn`, `FileUrl`, `HashOfZipFile`,
`BuildType`. Seven rows contain six distinct production versions, with identical
duplicate 1.1.127 rows. Highest numeric version is **1.5.619**. Its URL and ZIP hash
match the Windows handoff:

`https://update.eposstorage.com/deviceupdates/IMPACT_860T_ANC_1.5.619.zip`

`1cb6fb0810add6816220826e89e65629d1bf88ae1c2d22cec00e7256c34668bd`

Saved response/provenance:
`captures/linux/20261014T131249Z-firmware-catalog/` (Git-ignored).
Catalog SHA-256: `429810771c0e492ac8a03b3d39e0d5687c9a0c58ba31147e3654549da6024269`.
The speculative `/api/v1/…` variant returned an HTML page-not-found response and
is not used. No registration, account-token reuse, or firmware transfer was performed.

## Independent native implementation

`ReleaseCatalog` parses only the observed product's Production channel, compares
three uint16 version fields numerically, requires a matching model/version ZIP
filename on the official HTTPS package host and a valid SHA-256 value. Identical
duplicates are accepted; conflicting duplicates, invalid production entries,
empty catalogs and oversized responses fail without claiming “up to date.”
Dates are metadata, not version ordering. This public catalog is distinct from
the Windows tenant's scheduled/incremental update API.

`FirmwareController` checks automatically once a supported connection has an
actual firmware read, again on reconnection/version change and every six hours.
Manual refresh is available. Results and requests are invalidated on disconnect;
an old connection's response cannot overwrite the current state. A newer version
raises a global notice and Home shortcut to Updates. No package is downloaded
merely because a release is found. Automation/screenshot modes suppress networking.

The Device settings version label was a literal `Firmware: unknown` even while
Updates received the live value. It now binds to the same identity controller,
as does Home. A fresh hardware read during this change returned **1.5.64**.

A temporary harness using production `DeviceDiscovery`, `SettingsController`
and `FirmwareController` completed the live read/check pipeline at
**2026-10-14T13:17:49Z**: installed **1.5.64**, latest **1.5.619**, status
`available`, `updateAvailable=true`. It performed no settings writes or upgrade.
All 13 Release and ASan/UBSan CTest suites, QML lint, clang-format 21 and relocated
installation checks passed. UI tests assert the firmware text on all three pages,
the global/Home update notice, navigation and invalidation after disconnect.

## Installation remains separate

See the subsequent [non-flashing protocol investigation](PROTOCOL_RESEARCH.md)
for the independently traced Web updater and remaining live-evidence gaps.

Catalog availability and archive integrity do not establish device compatibility,
ownership, image/offset semantics, validation-pending behavior or activation/recovery.
Those are still required for a reliable native installer. There is no simulated
upgrade, local-file installation or unverified transfer hidden behind the UI.
