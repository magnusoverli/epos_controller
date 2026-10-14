# Extended value evidence — EPOS Connect 8.7.0.52713

The initial USB captures establish message layouts and four working settings.
Static inspection of the matching Windows adapter establishes additional values.
**TalkThrough, Low ANC and sidetone outside 32–33% have not yet been exercised on
the Linux headset.** The derived fixture bytes are predictions, not captures.
No vendor binary is loaded, linked or required by the application or tests.

## Reproduction inputs

Extracted `Program Files/EPOS/EPOS Connect/dciadapter.dll` from the archived
EPOS Connect installer, 6,193,520 bytes, PE32/i386, preferred image base
`0x10000000`, SHA-256:

```text
93d4c97315d9a87acd7bb9748092b212222387a74536e437281736dbc31a98ad
```

All addresses below are virtual addresses at that image base, not file offsets.
For example, inspect the sidetone conversion without executing vendor software:

```sh
sha256sum /path/to/dciadapter.dll
objdump -d -Mintel --start-address=0x103014aa --stop-address=0x103014f0 /path/to/dciadapter.dll
```

The extracted UI `dist/browser/main-KEYLTUWD.js` supplies a sidetone slider with
`floor:0,ceil:100` and emits its integer value. The adapter accepts a normalized
fraction; its setting IDs and C++ classes are evidence labels only, not runtime
interfaces in this project.

## Sidetone

- Static label initialization associates `Sidetone_Osbourne` with setting `0x2000`
  (`0x1000adc6–0x1000addf`).
- `0x103014aa–0x103014e5` converts numerator `n` / denominator `d` to
  `(255*n + floor((d+1)/2)) / d`, using integer division. When `d == 255`, the
  numerator is passed through. Results above 255 are rejected. The setter calls
  interface slot `0x100`.
- The `QtilSecomParser` vtable at `0x104873f4`, slot `0x100`, resolves to
  `0x102fc9f0`. It forwards the byte to generated-parser slot `0x114`.
- The generated-parser vtable at `0x104ba7d8`, slot `0x114`, resolves to
  `0x10320aa0`. At `0x10320b20–0x10320b3a`, it serializes group `0x0a`,
  operation `0x0b`, then the supplied value, matching the captured sidetone setter.

For a percentage this gives `floor((255*p + 50)/100)`: 0→0, 1→3, 10→26,
30→77, 32→82, 33→84, 50→128, 75→191, 99→252, 100→255. The two captured
setters independently agree. Application display rounds raw values to the nearest
integer percentage; it preserves noncanonical readings, e.g. `32% (raw 81)`.
Viewing or reapplying the same percentage never silently normalizes 81 to 82.

## ANC mode / TalkThrough

- The label table identifies `0x8071` as Off, `0x8072` as Active Noise
  Cancellation, `0x8073` as `Talk_Through_Off_Osbourne`
  (`0x1000d917–0x1000d96c`; the last string is at `0x1045b4b0`).
- The mode getter at `0x10304b20` maps native 0/1/2 to those three IDs.
- The setter at `0x10304e00` maps the IDs back to 0/1/2, then calls slot
  `0x1fc`. The first vtable resolves that to `0x102fba30`, which forwards
  0/1/2 unchanged through generated-parser slot `0x90`.
- That slot resolves to `0x10315b40`; `0x10315bbc–0x10315bd6` serializes
  group `0x17`, operation `0x1f`, and the value. Captures already establish
  0 and 1 on this same operation; **TalkThrough is therefore 2**.

ANC-dependent controls remain disabled in TalkThrough, just as in Off mode.

## ANC level / Low

- `0x1030c0b1–0x1030c0cc` recognizes adjustable ANC setting `0x8080` and
  queries its maximum through interface slot `0x208`.
- That slot resolves to `0x102f9680`, forwarding through generated-parser slot
  `0x80`, function `0x103152a0`. At `0x10315320–0x1031532e`, it serializes
  maximum-level query **`17:1b`**.
- The reviewed enumeration capture, frames 197/199, and final-reconnect capture,
  frames 193/195, contain `17001b00` / `17001b000200`: maximum **2**.
- `0x1030c163–0x1030c190` constructs the range with minimum **0**, increment
  **1**, and that returned maximum. The application exposes the three levels
  as Low=0, Middle=1, High=2; Middle/High are independently captured and live-tested.
- The setter wrapper at `0x102fb930` forwards the level without an offset through
  generated-parser slot `0x78`, function `0x10315820`. At
  `0x103158a0–0x103158ba` it serializes **`17:19`**, followed by the supplied
  level. This is the already captured level setter.

The maximum is model/firmware evidence from this headset, not a universal EPOS
capability. Support remains scoped to `1395:018e`; broader firmware validation
and live capability querying remain future work.

## Derived fixture provenance

`tests/SettingsDerivedFixtures.h` adds immutable requests/replies using these
values in captured layouts. The synthetic out-of-range read (sidetone 256) tests
unknown-value handling; it is not an advertised device capability. Response
status shapes follow the captures and do not establish undocumented error codes.

The CRC bytes were calculated offline by polynomial long division, independently
of the production byte-loop encoder. For body bytes `b`, divide
`(int.from_bytes(b, 'big') << 8) XOR (0xff << (8*len(b)))` by polynomial `0x107`
over GF(2); the remainder is the CRC. This method was cross-checked against the
captured `0a000c00` (CRC `b1`) and `170020000100` (CRC `35`) before generating
the extended expectations. The fixtures remain separate from the 17 original
capture pairs, so the raw-capture verification script cannot mistake them for
observed packets.
