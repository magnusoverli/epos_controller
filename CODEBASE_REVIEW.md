# Codebase review

Reviewed 2026-10-13 against commit `0eb4d46`.

## Resolution update — 2026-10-13

The original audit below is retained as a historical baseline; its line numbers
and descriptions refer to the reviewed commit, not the refactored implementation.
All 15 findings have an implementation or explicit scope decision:

| Finding | Resolution | Regression/evidence |
| --- | --- | --- |
| F01 | One declarative CLI grammar produces a typed launch mode before application construction. Only parsed Qt options are forwarded; JSON modes are exclusive; screenshot + smoke honors warnings. | Process-level CLI table covers headless help/errors, option values, `--`, Qt platform forwarding, and screenshot completion. |
| F02 | Skip USB interfaces and unrelated product reads; enumerate HID once per scan; cache layouts by connection/descriptor identity; refresh permissions; debounce udev with fallback polling. | FIFO fixtures catch accidental reads; generation/cache tests; maintained benchmark; current-topology identity opens reduced from 144 to 25, with failures reduced from 96 to zero. |
| F03 | Typed scan/issue/device/interface results and bounded binary/text readers retain missing, empty, permission, read, size, enumeration, and parser errors. Enumeration failures retain explicitly stale data. | Filesystem error/limit tests, stable JSON error codes, unavailable/stale-state regression. |
| F04 | Custom controls now expose distinct focus, hover, pressed, disabled, and selected visuals; control text inherits its font; dialog close restores focus. | Keyboard activation, focus-border assertion, dialog focus return; Help action stays outside its scrolling text viewport. |
| F05 | Default installation deploys matching Qt libraries/QML/plugins with relative paths; desktop assets, MIT license (chosen by the owner), and dependency notices are installed. A distro-managed mode remains explicit. | Relocated-install script verifies Qt resolution and all pages; clean Ubuntu runtime image is defined without system Qt. SDK-specific source/license material remains a documented prerequisite for publishing a binary release. |
| F06 | Host-independent Qt Test fixtures exercise scanning, selection, errors, timers, coalescing, navigation, lazy loading, scrolling, plain-text names, and diagnostic copy/refresh; CLI errors assert exact modes/messages. | Seven CTest entries now contain substantive behavioral cases; separate normal/high-DPI interaction and real startup checks replace redundant page-only startup tests. |
| F07 | One owned worker performs scanning; at most one follow-up refresh is queued; forced refreshes discard stale in-flight results; shutdown joins the provider. Diagnostics exposes asynchronous busy/snapshot state. | Semaphore-controlled test proves worker execution, 20 requests coalescing to one follow-up, and suppression of the invalidated snapshot. |
| F08 | Hardware-derived labels and selection delegates explicitly use plain text; long names wrap/elide within bounded widths. | Long markup-like fixture name remains literal at the minimum window size. |
| F09 | Typed domain snapshots plus a typed `SelectedDevice` QObject replace page-level dynamic device maps. The selector receives only lightweight name/connection choices; serialization is a separate adapter. Help cards have stable instances. | Selection/disconnect/stale tests and typed QML lint; a full list-model framework is unnecessary for this small choice list. |
| F10 | Pages load on first visit and retain their state; settings uses a virtualized ListView without recycling focused controls. | Tests verify inactive initial loaders, retained settings scroll position, both settings catalogs and dropdown identity during scrolling. Static Canvas artwork is retained because no continuous repaint or measured artwork bottleneck was found. |
| F11 | JSON serialization returns UTF-8 bytes; only the GUI converts to QString. Diagnostics includes capture time/status/issues and copies the displayed snapshot. | CLI JSON checks and stale-snapshot clipboard regression. |
| F12 | Selection tracks process-local connection generations, survives ordering changes, and becomes disconnected on replacement/removal until the user selects a current connection. | Earlier-sorted device, same-port replacement, stale state, and explicit reselection tests; future transport ownership contract is tracked in CONTRIBUTING.md. |
| F13 | Supported Qt minimum is deliberately raised to verified 6.12; CMake checks required library features, strict warnings cover handwritten core/startup, parser-only builds avoid Qt/MOC, and CI defines build/lint/sanitizer/fuzzer/deployment checks. | Qt 6.12 builds and warning-free lint; standalone GCC and Clang parser builds. Remote CI execution depends on repository hosting. |
| F14 | Shared independent capture, exact resource-boundary acceptance, structured mutations, additional invariants, and a seeded Clang libFuzzer target. | Local 30-second ASan/UBSan fuzz run completed 46,288 inputs without a finding. The measured-fast parser map is retained; no speculative allocator/container rewrite. |
| F15 | Theme/focus tokens, translated-source markers, a shared page enum/application name, predictable SDK-wrapper usage and extra prefixes, clang-format 21 normalization/check, tracked contributor guidance, and corrected stale local Git claims. | Formatting/script checks; normal/high-DPI UI tests. English remains the only shipped language; the tracked guide explains the translation workflow rather than claiming untranslated locales. |

The selection and worker changes establish contracts for future commands; they do
not add unverified settings writes or firmware operations. Native hardware
unplug/replug and desktop/display-driver validation remain distinct from automated
fixture and offscreen checks.

### Final verification

- Release and ASan/UBSan: **7/7 CTest suites passed** in each configuration.
  UI interaction runs at 14/20-pixel application fonts and normal/2× display scale.
- An isolated copy of the 72 maintained source files, excluding ignored research
  and build directories, built successfully and passed all seven suites.
- QML lint passed with zero warnings; clang-format 21, shell/Python syntax,
  CMake preset JSON, and Git whitespace checks passed.
- Relocated installation passed Qt-library resolution, diagnostics, every page,
  and diagnostics-dialog startup checks without inherited SDK path overrides.
- A clean Ubuntu 26.04 container with native runtime dependencies but no system
  Qt passed diagnostics and all five GUI startup checks, with no stderr output.
  The container runs unprivileged, offline, with the installed tree read-only.
- GCC and Clang parser-only builds passed; the seeded Clang ASan/UBSan fuzzer
  completed 46,288 inputs in 31 seconds without a finding.
- A supported headset was present during implementation verification: live
  descriptor discovery and screenshots of all four pages succeeded. Physical
  unplug/replug and native desktop interaction were not exercised.
- On the same current host topology, the original executable attempted 144 USB
  vendor/product opens (96 failed); the updated executable attempted 25 (none
  failed). The maintained warm fixture benchmark measured approximately
  48/43/125 µs median for 0/10/50 unrelated devices in one run. These timings are
  noisy and include added identity/error handling; the syscall reduction is the
  stronger performance evidence.

GitHub-hosted CI has been defined but not run remotely. Public binary publication
still requires the applicable SDK licensing/source materials described in
`THIRD_PARTY_NOTICES.md`; the local staged runtime is not a published release.

## Executive summary

The application is already small, well separated, and modern: C++23 value types,
RAII, an independently testable HID parser, injected discovery, and compiled QML.
A broad rewrite would add more risk than value. The best next changes are:

1. Fix command-line application selection, which has reproducible abort cases.
2. Remove unnecessary filesystem probes from discovery before optimizing parsing.
3. Preserve filesystem errors instead of presenting every discovery failure as
   an absent headset.
4. Add deterministic interaction/CLI tests and visible keyboard focus.
5. Establish and test an installation/runtime-dependency contract before release.

The existing Release and ASan/UBSan suites both pass all 10 tests; QML lint is
clean. These checks miss several behaviors demonstrated below. No application
implementation was changed during this investigation.

## Scope and method

Read every tracked file: **41 files, 1,600 lines** before this review was added:

| Area | Files | Lines |
| --- | ---: | ---: |
| Root build configuration, formatting, README, changelog | 7 | 263 |
| C++ application and headers | 17 | 600 |
| Compiler/build options | 1 | 27 |
| QML | 12 | 452 |
| SDK wrapper | 1 | 26 |
| Tests | 3 | 232 |

Also inventoried ignored directories and consulted the local architecture,
engineering, toolchain, and planning documents. The two archived third-party
TypeScript files were read; they are reference material, not application
dependencies. Generated build outputs and vendor ZIP/MSI/PDF/HTML archives are
not maintained application source and were not treated as refactoring targets.

Validation used fresh out-of-tree builds, source inspection, CTest, QML lint,
headless CLI edge cases, syscall tracing, a temporary fixture/measurement
harness, rendered QML inspection, and a staged installation. The harness reused
the actual Release application objects/resources with a replacement entry point;
it did not alter production source. Temporary artifacts are under
`/tmp/opencode/epos-audit-*`.

This covers the complete maintained source inventory, not every possible runtime
execution. The live scan found no supported headset. Physical hotplug, real HID
permission changes, native desktop interaction, Qt 6.8, alternative compilers,
and clean-machine deployment remain unverified. Offscreen rendering used Qt's
software backend. The measurements below are host-specific warm-cache samples,
not device-I/O latency measurements or guaranteed speedups.

## Verified baseline

| Check | Result |
| --- | --- |
| Fresh Release configure/build | Pass, GCC 15.2 / Qt 6.12.0 / CMake 4.4.4; no compiler warnings observed |
| Release CTest | 10/10 pass |
| Fresh Debug ASan/UBSan configure/build and CTest | 10/10 pass; no sanitizer findings in these runs |
| `headset-controller_qmllint` | Pass |
| `bash -n scripts/with-sdk.sh` | Pass |
| ELF inspection | PIE, GNU_RELRO plus BIND_NOW, non-executable stack, stack-protector reference |
| Pure parser test linkage | No Qt shared-library dependency |
| Four pages at 960 × 650 with a fixture headset | Rendered and inspected |
| Application-settings, diagnostics, simulated disconnect | Rendered and inspected |
| Custom-button keyboard-focus rendering | Defect reproduced; see F04 |
| Headless CLI edge cases | Defects reproduced; see F01 |
| Default staged installation | Runtime failure reproduced; see F05 |

### Performance observations

The synthetic scanner fixture contained one matching USB device, one HID
interface with the captured descriptor from `tests/hid_test.cpp:20`, and the
specified number of unrelated USB devices. Each scan result was compared with
the original snapshot. No actual HID node was opened.

| Operation | Samples | Observed time |
| --- | ---: | ---: |
| Full fixture scan, 0 unrelated devices | 1,000 | Median 43.93 µs; p95 52.09 µs |
| Full fixture scan, 10 unrelated devices | 1,000 | Median 88.73 µs; p95 105.91 µs |
| Full fixture scan, 50 unrelated devices | 1,000 | Median 272.84 µs; p95 320.71 µs |
| Parse captured 65-byte descriptor | 100,000 | Mean 0.211 µs |
| Serialize fixture diagnostics, including current UTF-8 round trip | 10,000 | Mean 24.554 µs |
| Launch process with `--version` | 60 | Median 9.80 ms |
| Launch process with `--diagnostics`, no supported device | 60 | Median 10.52 ms |

The syscall trace of one real discovery scan showed **128 USB attribute-open
attempts: 64 `idVendor` and 64 `idProduct`**. Of those, **84 failed** because 42
USB interface directories do not provide those device-level attributes. There
were 22 device directories and no matching vendors.

The current 1.5-second poll repeats this work roughly 40 times per minute.
Skipping interface directories and short-circuiting after a nonmatching vendor
would reduce these particular attribute opens from 128 to 22 on this topology.
That is an approximately 83% reduction in this category of calls, not an 83%
application speedup. Current measured scans are short; UI stalls have not been
established by these measurements. Parser micro-optimization is particularly
unlikely to produce a user-visible improvement.

## Findings and recommendations

Priority definitions: **P1** = address soon or before shipping the affected
workflow; **P2** = worthwhile next refactoring; **P3** = incremental or conditional
improvement. Effort is relative: small, medium, or large.

### F01 — Application selection does not follow the CLI grammar

**P1 · confirmed defect · small/medium**

References: `app/main.cpp:19-27`, `app/main.cpp:35-65`,
`CMakeLists.txt:69-72`.

`needsGui()` independently searches raw argument strings before
`QCommandLineParser` interprets them. It neither recognizes every help option nor
distinguishes option values from options.

Reproductions on the fresh Release executable:

| Invocation | Result |
| --- | --- |
| `QT_QPA_PLATFORM=invalid ... --help` | Exit 0 |
| `QT_QPA_PLATFORM=invalid ... --help-all` | SIGABRT trying to initialize a GUI platform |
| `... --screenshot --help` | SIGABRT: `Cannot create window: no screens available` |
| `... --screenshot --version` | Same abort |
| `QT_QPA_PLATFORM=invalid ... --bogus` | Platform abort before a useful CLI error |
| `QT_QPA_PLATFORM=invalid ... --page invalid` | Platform abort before page validation |

For the screenshot cases, the real parser consumes the following token as the
filename, while the pre-scan chooses `QCoreApplication`. The program subsequently
tries to create a window without a `QGuiApplication`.

**Recommendation:** define command-line options once, parse/validate an explicit
argument list before selecting the application class, and produce a typed launch
mode. Deliberately preserve support for Qt's own GUI arguments when reorganizing
parsing. Avoid simply adding another string comparison to `needsGui()`.

Also decide whether `--diagnostics --list-devices` should be rejected or have
documented precedence; it currently silently chooses diagnostics. Define the
interaction between screenshot and smoke modes: the screenshot branch currently
ignores the smoke mode's QML-warning failure flag (`app/main.cpp:88-96`).

**Acceptance:** argument-table tests assert exit code, stdout/stderr, option-value
handling, `--` handling, all help forms, and conflicting modes without requiring
a display. Invalid inputs must not terminate by signal.

### F02 — Discovery repeats avoidable filesystem and descriptor work

**P2 · measured opportunity · small initially; medium for caching/events**

References: `app/SysfsDeviceScanner.cpp:22-37`,
`app/HidInspection.cpp:24-45`, `app/DeviceDiscovery.cpp:18-22`.

The scanner reads both identity attributes from every directory, including USB
interfaces and vendors already known not to match. Every matching device causes
another full traversal of `hidRoot`, canonicalization, descriptor read/parse,
interface-ancestor search, and permission checks. Even unchanged polls construct
new nested variant snapshots before equality suppresses the notification.

**Recommended sequence:**

1. Skip sysfs interface entries and read `idProduct` only after the vendor matches.
   Keep directory ordering deterministic so comparison and selection stay stable.
2. Separate cheap presence/identity discovery from detailed HID inspection.
   Cache immutable descriptor layouts for a connection lifetime and explicitly
   refresh them for diagnostics or topology changes.
3. If multiple headsets become supported, enumerate HID ancestry once per scan,
   then associate interfaces to devices instead of rescanning for each device.
4. Consider a udev event source with event coalescing and an occasional fallback
   rescan. An ordinary filesystem watcher is not a general replacement for
   Linux device-event monitoring on sysfs.

Do not cache solely by USB port name: that path can be reused after reconnect.
Track connection generation/identity, invalidate removed devices, and define how
permission changes are refreshed. Byte limits and deterministic ordering must
survive the refactoring.

**Acceptance:** syscall/scan benchmarks plus reconnect, same-port replacement,
permission refresh, unrelated-HID, and unchanged-notification tests.

### F03 — Missing devices and discovery failures are indistinguishable

**P2 · confirmed design limitation · medium**

References: `app/SysfsDeviceScanner.cpp:9-16,24-30`,
`app/HidInspection.cpp:26-36`, `app/HidDescriptorPresentation.cpp:23-29`,
`tests/discovery_test.cpp:33`.

An absent/unreadable USB root produces the same empty list as a valid scan with
no supported headset. Attribute open failures, read errors, empty data, and
oversized values collapse into an empty string. HID open failures become an empty
descriptor and lose their filesystem cause. The presentation adapter retains an
error message/offset but drops the parser's typed error code.

**Recommendation:** introduce a typed scan result containing devices, scan
status, and bounded per-device/interface issues. Distinguish enumeration failure,
transient removal, permission failure, oversized input, and parse failure.
Preserve stable machine-readable error codes at the JSON boundary. Decide when
previous device data should remain visible as stale rather than disappear.

A shared small bounded-read helper can remove the inconsistent attribute versus
interface-number read policies, but it should carry errors rather than merely
deduplicate file-opening code. Keep expected hotplug races distinct from a
persistently unavailable backend.

**Acceptance:** fixtures for missing roots, disappearing entries, unreadable
files, exact read limits, and malformed descriptors; verify both JSON and UI
status, not just list length.

### F04 — Custom buttons have no visible keyboard-focus state

**P2 · confirmed defect · small**

References: `qml/OutlineButton.qml:8-16`, `qml/Main.qml:64-75`.

The custom backgrounds replace the style's normal focus visuals without adding
a `visualFocus` treatment. In the rendered Home fixture, forcing tab focus on
the visible Device settings button made `activeFocus` and `visualFocus` true, but
the focused and unfocused screenshots were byte-identical.

The custom `Text` in `OutlineButton` also does not explicitly follow
`control.font`, weakening typography consistency. Navigation uses the selected
page color, which does not separately identify keyboard focus.

**Recommendation:** add a visible focus border/ring, bind custom label typography
to the control, and distinguish disabled, pressed, hovered, focused, and selected
states. Centralize those colors in `Theme.qml`. Preserve the existing useful
`Accessible.name` on navigation buttons.

**Acceptance:** keyboard traversal and activation across pages and the modal
dialog, visible focus at each step, and sensible focus return after closing it.

### F05 — Default installation does not establish matching Qt dependencies

**P1 before distribution · reproduced deployment gap · medium/large**

References: `CMakeLists.txt:74`, `README.md:25,43,87`,
`scripts/with-sdk.sh:19-26`.

Staging the clean Release build with the existing install rule succeeded. Running
the installed executable failed immediately:

```text
libQt6Core.so.6: version `Qt_6.12' not found
```

The installed executable's runtime path was `$ORIGIN:$ORIGIN/../lib`, but no Qt
libraries were installed there. It resolved the host's older Qt instead of the
SDK used for compilation. The build-tree executable correctly resolves the SDK.
The README already says the output is not a standalone bundle; the experiment
confirms that this is a real distribution boundary, not just missing prose.

**Recommendation:** choose either a distro package with explicit compatible Qt
runtime dependencies or a deployment bundle containing the required Qt libraries,
plugins, and QML modules. Test an installed executable in a clean environment.
An absolute SDK RPATH is appropriate only for a documented local development
install, not a portable release.

Also provide the chosen project license and applicable Qt/third-party notices;
there is currently no tracked project license. Add desktop entry/icon integration
when defining the Linux package. Do not ship archived vendor materials merely
because they exist locally.

### F06 — UI/CLI tests do not exercise important behavior

**P2 · confirmed coverage gap · medium**

References: `CMakeLists.txt:50-72`, `app/main.cpp:94-96`,
`tests/discovery_test.cpp`, `tests/hid_test.cpp`.

- UI tests inspect the actual host, so a passing run does not establish connected
  UI coverage. This audit's host had no supported headset.
- Smoke tests exit after 800 ms, before the 1,500 ms discovery timer fires.
  They do not verify automatic refresh behavior.
- No test clicks through navigation, toggles application/device settings,
  validates scrolling, copies diagnostics, or checks focus restoration.
- `cli-invalid-page` combines invalid page data with an already forbidden
  CLI/GUI combination. It could still pass if numeric page validation broke.
  `WILL_FAIL` does not verify the intended error text or exact exit code.
- GUI success is a warning-free timed startup, not an assertion that a frame or
  an expected connected/disconnected state was presented.
- The older integration tests return unexplained numeric codes, and some fixture
  setup operations do not check success. The parser tests have substantially
  better named failure messages already.

**Recommendation:** add a small Qt Test/Qt Quick Test interaction layer with
injected snapshots and explicit ready/state assertions. Test timer wiring with
an injected interval and event-loop synchronization rather than sleeps. Keep one
real startup smoke test and a separate optional live-hardware check.

Reuse the existing named assertion approach or a test framework in integration
tests. Assert complete diagnostic fields and snapshot-copy consistency. Use a
table-driven CLI suite that verifies the error being tested. Make fixture
creation failures distinguishable from production behavior failures.

### F07 — GUI-thread discovery is a future latency boundary

**P2 before adding device I/O · architecture opportunity · medium**

References: `app/DeviceDiscovery.cpp:7-19`,
`app/DiagnosticsController.cpp:10-12`, `qml/DiagnosticsDialog.qml:11-12`.

Construction, timer refresh, manual refresh, and opening diagnostics all perform
the full scan synchronously on the GUI thread. Current measured fixture scans
are inexpensive; there is no evidence here that adding threads would materially
speed this prototype. Nevertheless, byte limits do not bound syscall latency,
and future USB reads must not inherit this execution model.

**Recommendation:** when responsiveness measurements or settings I/O justify it,
run scanning/transport work in an owned worker and publish value snapshots on the
GUI thread. Permit one scan in flight, coalesce refresh requests, track connection
generations, discard stale results, and define shutdown behavior. Expose loading
and failure states to diagnostics instead of retaining a synchronous string-return
contract that cannot represent an in-progress refresh.

Do not introduce one thread per poll, concurrent access to the move-only scanner,
or unbounded queued refreshes. Preserve the good current ownership relationship:
the injected discovery/controller outlive the QML engine.

### F08 — Hardware-derived names are interpreted as formatted text

**P2 · reproduced presentation defect · small**

References: `qml/HomePage.qml:28`, `qml/UpdatesPage.qml:29`,
`qml/HelpPage.qml:15,33`, `qml/Heading.qml`.

QML labels default to automatic text-format detection. A fixture device named
`<b>MARKUP TEST</b>` rendered as bold text instead of displaying the literal
device name. This is a presentation correctness finding; no code execution or
network access was demonstrated.

**Recommendation:** set `Text.PlainText` explicitly on labels displaying device
metadata. Give dynamic names bounded width with wrapping or elision; the Home
heading and Updates device row currently lack a complete long-name policy.
Test long strings and markup-like names without changing the actual device.

### F09 — Dynamic variant snapshots limit type checking and change granularity

**P2 as the domain grows · refactoring opportunity · medium**

References: `app/DeviceDiscovery.h:10-16`,
`app/SysfsDeviceScanner.cpp:33-37`, `app/HidInspection.cpp:36-45`,
`qml/Main.qml:13`, page `required property var device` declarations.

String keys cross scanning, JSON, and QML without compiler-checked field
contracts. Any nested HID change invalidates the whole device snapshot. QML
tooling cannot verify fields accessed through these `var` values as effectively
as typed properties. `HelpPage.qml:13-17` recreates its JavaScript model when
dependent device state changes.

**Recommendation:** introduce small typed device/interface/status value objects
when implementing richer errors or real settings, with explicit QML and JSON
adapters. Start with a typed selected-device presentation object. Adopt
`QAbstractListModel` only when selection, many devices, or granular updates need
it; a one-device prototype does not justify a model framework by itself.

Qt containers are implicitly shared. Returning `QVariantList` by value and
assigning `m_devices = latest` are not automatically deep-copy bottlenecks; avoid
unmeasured const-reference/lifetime rewrites in the name of performance.

### F10 — All pages and settings delegates are constructed eagerly

**P3 · observed structure; speedup unmeasured · small/medium**

References: `qml/Main.qml:29-46`, `qml/SettingsPage.qml:41-73`,
`qml/HeadsetArt.qml:7-30`.

`StackLayout` switches visibility, not object creation. The fixture harness found
248 QObject descendants under the window regardless of the initially selected
page. Settings uses a `Repeater`, so all rows exist even when offscreen, and both
headset canvases are part of the instantiated UI.

**Recommendation:** if startup or richer pages become measurable costs, introduce
page `Loader`s, optionally retaining visited pages. Consider a `ListView` when
the settings inventory grows or delegates become interactive/expensive. Preserve
page state, scroll position, and keyboard accessibility across loading changes.

The current inventory is only 25 device entries and four application entries, so
virtualization is optional. The Canvas repaints on size changes, not an explicit
continuous timer. Replacing it with SVG/Qt Quick Shapes is an asset-maintenance
choice until profiling demonstrates a rendering benefit; SVG may add a deployment
dependency and must preserve the software-backend behavior.

### F11 — Diagnostics performs an avoidable encoding round trip

**P3 · confirmed small inefficiency · small**

References: `app/Diagnostics.cpp:8-16`, `app/main.cpp:69`,
`app/DiagnosticsController.cpp:10-12`.

`QJsonDocument::toJson()` produces UTF-8 bytes, `Diagnostics::toJson()` converts
them to `QString`, and the CLI converts them straight back to UTF-8. The work is
small and occurs on demand, so it is not a primary optimization target.

**Recommendation:** return a `QJsonDocument` or `QByteArray` from the serializer,
and convert to a display string only in the GUI controller. Preserve formatting,
schema compatibility, stdout error handling, and the correct existing behavior
of copying the displayed snapshot rather than a newly generated report.

If support needs grow, add snapshot time/scan status and machine-readable issues.
Keep diagnostic export read-only and continue distinguishing USB revision from
unknown firmware.

### F12 — Selection identity needs a contract before writable features

**P2 before device commands · known functional boundary · medium**

References: `qml/Main.qml:13`, `app/SysfsDeviceScanner.cpp:25,33`,
`README.md:13`.

The UI always displays the first name-sorted result. Adding/removing a device
that sorts earlier can silently change the selected headset, and the USB path
key can be reused by a different physical connection. This is documented and
acceptable for the current read-only view, but unsuitable as implicit identity
for asynchronous settings or firmware operations.

**Recommendation:** define selection, connection generation, disconnect state,
and in-flight request ownership before introducing writes. Keep verified
capabilities separate from the static `SettingsCatalog` inventory. Preserve the
current separation between discovery, future settings transport, and updater;
do not grow `DeviceDiscovery` into a universal device manager.

### F13 — Build and CI can better enforce the supported toolchain contract

**P2 · maintainability/compatibility gap · medium**

References: `CMakeLists.txt:10-18,50-68`,
`cmake/ProjectOptions.cmake`, `CMakePresets.json`, `README.md:20-25`.

- Qt 6.8 is declared but only 6.12 was verified. Add a minimum-version build and
  a current-SDK build, or revise the supported minimum deliberately.
- A compiler accepting `-std=c++23` need not provide both required library
  facilities. A configure-time compile check for `std::expected` and
  `std::move_only_function` would turn obscure build failures into useful errors.
- No tracked CI currently runs the existing presets/lint. A useful pipeline is
  Release build/tests, QML lint, sanitizer tests, and a staged-install check for
  the chosen deployment target. A clean source-only checkout should be tested.
- Strict conversion/shadow warnings cover only the parser. Evaluate extending
  them to handwritten core code without drowning generated Qt code in warnings.
- The pure parser target inherits automatic MOC/UIC despite containing no Qt
  objects; the fresh build generated an empty MOC unit. Disable those properties
  for that target as is already done for its test.
- Configuring even the Qt-independent parser requires the GUI Qt packages at the
  top level. An optional parser-only configuration would help future fuzz/portable
  builds, but is not required for current desktop development.
- Repeated UI test environment/timeout settings can be centralized in a small
  CMake helper. Retain target-local hardening and sanitizer options.

Use LTO/IPO only after build-size/startup measurements justify it. The project is
already on a modern language/library baseline; upgrading syntax or introducing
C++ modules/coroutines is not itself a performance plan.

### F14 — Parser validation is solid; extend tests before replacing containers

**P3 · assurance opportunity · small/medium**

References: `app/HidDescriptor.cpp`, `app/HidDescriptor.h`,
`tests/hid_descriptor_test.cpp:54-105`.

The parser bounds input size, nesting, multiplication, and accumulated report
length; uses safe bytewise decoding; aggregates bits before rounding; and returns
typed errors without exposing partial layouts. No parser correctness or memory
safety defect was demonstrated in this review.

Its map has a bounded key domain, and the measured captured-descriptor parse is
approximately 0.2 µs. A dense `3 × 256` counter array would avoid node allocations
but initialize roughly 6 KiB even for a six-report descriptor. Neither it nor a
custom allocator is justified without representative comparative measurements.

**Recommendation:** add coverage-guided fuzzing around the existing pure API when
practical. Retain the deterministic corpus, but recognize that its random inputs
are only 0–255 bytes and often fail early. Add structured valid descriptors,
exact-limit acceptance cases, multiple report kinds/IDs, usage-page deduplication,
and stronger successful-result invariants. Share the captured fixture between
tests while retaining independent expected layouts.

Keep documenting that `valid` means accepted by this layout inspector, not full
HID semantic conformance or authorization to send commands. Do not expand it
into a full HID validator without a consumer needing that contract.

### F15 — Small presentation and repository-maintenance improvements

**P3 · maintainability opportunities · small individually**

References: `qml/Theme.qml`, `qml/SettingsCatalog.qml`, `qml/Main.qml`,
`qml/SettingsPage.qml`, `app/BuildInfo.h.in`, `scripts/with-sdk.sh`, `.gitignore`.

- Shared colors still appear as literals across controls/pages despite the theme
  singleton. Centralize semantic colors and a small typography/spacing vocabulary
  when updating focus and layout; avoid a general-purpose UI framework.
- User-facing strings are not marked for translation. Use `qsTr()`/`tr()` and a
  translation workflow if application language becomes an actual supported
  feature. The static catalog is correctly described as inventory, not readback.
- Page numbers `0..3` are duplicated in C++, QML, and tests. A named page enum or
  shared navigation definition would make future page additions safer.
- Build version already has one source of truth. Application name is still
  repeated in `main.cpp` and `Diagnostics.cpp`; centralize it if branding changes.
- Fixed margins and preferred widths work for the inspected 960 × 650 fixtures,
  but larger fonts, long names, localization, and native high-DPI rendering need
  explicit checks. Help/Settings scrolling is intentional; content below the
  viewport is not automatically an overflow defect.
- The SDK wrapper correctly quotes paths and uses `exec`; it intentionally pins
  a local layout. Check argument presence before SDK availability so a bare
  invocation always gives usage. Document or preserve an opt-in additional
  CMake prefix if dependencies are later added, rather than silently discarding
  all caller-provided prefixes.
- `.clang-format` exists but the compressed one-line style is not consistently
  aligned with it. Apply formatting in a separate mechanical change and add a
  formatting check if desired; avoid obscuring behavioral review with mass churn.
- `.gitignore` excludes all `docs/` and `AGENTS.md`. Essential contributor/build
  contracts must live in tracked files or the ignore policy should be narrowed.
  Local planning/review documents still contain stale claims that no Git
  repository exists. They should not be the authoritative release instructions.

## Recommended implementation order

| Stage | Work | Success criteria |
| --- | --- | --- |
| 1: Correctness and usability | F01 CLI grammar; F04 focus; F08 plain-text names; targeted F06 tests | Reproductions become regressions; no signal exits; keyboard focus is visible |
| 2: Cheap execution improvements | Initial F02 filtering; optional F11 serialization cleanup | Reduced measured attribute opens; unchanged JSON/device behavior |
| 3: Better backend contract | F03 typed errors; selected parts of F09; connection identity from F12 | Distinct absent/error/stale states; reliable reconnect semantics |
| 4: Sustained responsiveness | F02 cache/events and F07 worker when justified | Bounded/coalesced work; stale results rejected; reconnect tests pass |
| 5: Release foundation | F05 deployment, F13 CI/minimum-version validation | Clean-checkout build and clean-environment installed startup |
| 6: Growth-driven optimization | F10 lazy views; F14 fuzzing; remaining F15 cleanup | Improvements supported by measurements or concrete feature needs |

Retain the good existing module boundaries, move-only scanner injection, typed
parser, bounded metadata reads, explicit unsupported-feature UI, and snapshot
copy semantics. Prioritize removing repeated work and clarifying contracts over
introducing more abstraction or replacing already fast code.

## File coverage index

Every tracked file at the reviewed commit is included below; references link
coverage to findings rather than implying that every file requires changes.

| Files | Assessment |
| --- | --- |
| `app/main.cpp` | F01, F06, F11; lifetime ordering and stdout handling retained |
| `app/BuildInfo.h.in` | F15; version generation is appropriate |
| `app/DeviceDiscovery.h`, `app/DeviceDiscovery.cpp` | F02, F07, F09, F12; injection/change suppression retained |
| `app/SysfsDeviceScanner.h`, `app/SysfsDeviceScanner.cpp` | F02, F03, F09, F12; configurable roots and bounded reads retained |
| `app/HidInspection.h`, `app/HidInspection.cpp` | F02, F03; canonical ancestry checks retained |
| `app/HidDescriptor.h`, `app/HidDescriptor.cpp` | F14; no demonstrated parser defect |
| `app/HidDescriptorPresentation.h`, `app/HidDescriptorPresentation.cpp` | F03, F09, F14; Qt-independent parsing boundary retained |
| `app/Diagnostics.h`, `app/Diagnostics.cpp` | F03, F11, F15; side-effect-free serialization retained |
| `app/DiagnosticsController.h`, `app/DiagnosticsController.cpp` | F06, F07, F11; displayed-snapshot copy retained |
| `app/QmlTypes.h` | F09; foreign registrations keep QML coupling out of core |
| `qml/Main.qml` | F04, F09, F10, F12, F15 |
| `qml/HomePage.qml`, `qml/UpdatesPage.qml`, `qml/HelpPage.qml` | F06, F08, F10, F15 |
| `qml/SettingsPage.qml`, `qml/SettingsCatalog.qml` | F06, F10, F12, F15 |
| `qml/DiagnosticsDialog.qml` | F06, F07, F11 |
| `qml/OutlineButton.qml` | F04, F15 |
| `qml/HeadsetArt.qml` | F10; no demonstrated continuous repaint problem |
| `qml/Theme.qml`, `qml/Heading.qml`, `qml/Panel.qml` | F08, F15; small components remain suitable |
| `tests/discovery_test.cpp`, `tests/hid_test.cpp` | F02, F03, F06, F14 |
| `tests/hid_descriptor_test.cpp` | F14; named assertions and deterministic corpus retained |
| `CMakeLists.txt`, `CMakePresets.json`, `cmake/ProjectOptions.cmake` | F05, F06, F13 |
| `scripts/with-sdk.sh` | F05, F15; syntax check passed |
| `.clang-format`, `.editorconfig`, `.gitignore` | F15 |
| `README.md`, `CHANGELOG.md` | F05, F13, F15; documented prototype limitations checked against implementation |

### Archived code boundary

The ignored `docs/third-party/genesys-headset-integration/sennheiser.ts` and
`types.ts` are disconnected excerpts with unavailable surrounding imports, not a
second buildable application. If reused in future research, note the immediate
Promise resolution before connection/acknowledgement, unchecked WebSocket sends,
and the inconsistent `ProductName` read versus `deviceName` assignment
(`sennheiser.ts:38,266-269`). These observations are not findings against the
current C++ application's runtime and do not justify editing the archive.

## Reproduction commands

From the repository root, using the already provisioned SDK:

```sh
bash scripts/with-sdk.sh cmake -S . -B /tmp/opencode/epos-audit-release \
  -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
bash scripts/with-sdk.sh cmake --build /tmp/opencode/epos-audit-release --parallel 4
bash scripts/with-sdk.sh ctest --test-dir /tmp/opencode/epos-audit-release --output-on-failure
bash scripts/with-sdk.sh cmake --build /tmp/opencode/epos-audit-release \
  --target headset-controller_qmllint

bash scripts/with-sdk.sh cmake -S . -B /tmp/opencode/epos-audit-sanitize \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DHEADSET_SANITIZERS=ON
bash scripts/with-sdk.sh cmake --build /tmp/opencode/epos-audit-sanitize --parallel 4
bash scripts/with-sdk.sh ctest --test-dir /tmp/opencode/epos-audit-sanitize --output-on-failure

strace -qq -e trace=openat,read,access,readlink,newfstatat,getdents64 \
  -o /tmp/opencode/epos-audit-cli.strace \
  /tmp/opencode/epos-audit-release/headset-controller --diagnostics

bash scripts/with-sdk.sh cmake --install /tmp/opencode/epos-audit-release \
  --prefix /tmp/opencode/epos-audit-install
/tmp/opencode/epos-audit-install/bin/headset-controller --diagnostics
```

The staged-install result depends on the host runtime environment. Temporary
measurement/rendering harness sources are `epos-audit-probe.cpp` and
`epos-audit-build-probe.py` under `/tmp/opencode`; screenshots use
`epos-audit-page-*`. These investigation artifacts are not a permanent benchmark
suite. Add maintained benchmarks alongside any performance refactoring whose
benefit needs ongoing verification.
