# Settings protocol fixture provenance

`tests/SettingsCaptureFixtures.h` contains independent packet excerpts from the
reviewed Windows run `epos-evidence-20261012-135151-reviewed`, collected October
12–13, 2026: IMPACT 860T ANC `1395:018e`, firmware 1.5.64, EPOS Connect
8.7.0.52713, DSEA SDK 12.14.0.7636. These are device-protocol observations, not
vendor source code or an implementation of the Windows application architecture.

Each string starts with report ID `01`, includes the header/body, and is followed
by zero bytes to a total of **107 bytes**. This zero tail was checked against the
complete raw packets. There is no USBPcap header or USB setup packet in a report.
Tests use these immutable expected bytes rather than generating expected replies
with the production codec. The simulated headset matches output against the same
independent records and provides captured responses. Additional synthetic
expectations live separately in `tests/SettingsDerivedFixtures.h`; their
[adapter evidence and limitations](ADAPTER.md) are documented explicitly.
`tests/SettingsEnumerationFixtures.h` adds 15 captured getters for the
[additional preference controls](EXTENDED.md), [limiter/wear controls](SENSORS.md)
and [prompt/Auto Pause controls](PROMPTS.md), including [voice-prompt language](LANGUAGE.md).
`SettingsActionFixtures.h` contains
the separately labeled synthetic Factory Reset transaction.

| Capture | SHA-256 | Relevant original frames |
| --- | --- | --- |
| `02-sidetone-a.pcapng` | `af14cfc3d14a48a02c972970b46c2ce86242f9dfa4892f6c30e0e2026455fe52` | Requests 1/5/9/13/17/21; replies 3/7/11/15/19/23 |
| `02-sidetone-b.pcapng` | `8228fef2fe0a619d56a7995a35c035be39bf38c75188d167eadc162662e3cec9` | Same request/reply frame positions; independent repeat |
| `03-anc-mode.pcapng` | `066b3027bee2fe6ca55586d5fec724c36df7e06a281acfa9239980e934d9204f` | Requests 5/9/13/23/27/31/39/43/47/57/61/65; reply associations in bundle transactions |
| `04-anc-adaptive.pcapng` | `dcae8b65a6a46a24ee2db9512a02ff516ce2760703ecb927da4a21c122d12dfe` | Requests 5/9/13/21/25/29; replies 7/11/15/23/27/31 |
| `05-anc-level.pcapng` | `ae1ae25d83af9bf1f01258f0ab1fe17c10c08bf713deff2c1d5e5da6a5588fe8` | Requests 5/9/13/21/25/29; replies 7/11/15/23/27/31 |

With the optional local evidence bundle, reproduce the cross-check:

```sh
python3 scripts/verify-settings-evidence.py captures/windows/epos-evidence-20261012-135151-reviewed
```

The script checks bundle hashes, actual USBPcap packet bytes, all 44 transaction
request/reply frame references, all 32 captured fixture pairs, and the CRC rule against the
broader enumeration/reconnect corpus. It does not require Wireshark or hardware.
The original bundle stays ignored and is not a build/test dependency.
