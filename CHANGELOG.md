# Changelog

## [Unreleased]
- Add a prebuilt Ubuntu 26.04 x86-64 release bundle and rewrite the README around installation and everyday use.

## [1.0.0] - 2026-10-15

First source release. See [release notes](releases/1.0.0.md) for features,
build instructions and hardware-validation limitations.

- Set application version to 1.0.0 and show it in the window title and header.
- Add versioned release notes and correct the CI OpenGL and fuzzer runtime dependencies.

### Development history

- Settings sections now start collapsed and support click/keyboard expansion; removed the Test notification button.
- Keep settings responsive during background readback; sidetone saves on mouse/touch release without Apply or keyboard adjustment, with clearer errors and navigation.
- Allow user-initiated installation of future official firmware releases using the captured update flow and live catalog hashes, without a version allowlist.
- Add capture-backed in-app firmware installation for IMPACT 860T ANC 1.5.64 → 1.5.619, with verified downloads, reboot tracking and interrupted-update protection.
- Headset access setup now includes the firmware HID interface, which the EPOS web updater needs to start an update.
- Documented non-flashing firmware protocol research, including ownership encoding, retry limits and corroborated transfer/validation uncertainties.
- Firmware now displays on Home and Device settings, checks EPOS's live catalog automatically and flags newer releases; removed saved-offer and local-ZIP controls.
- Fixed incremental packaging so newly imported QML modules, including file dialogs, are included in installed builds.
- Updates now loads saved firmware offers, downloads official packages over HTTPS, inspects local ZIPs asynchronously and compares versions with the selected headset.
- Added offline firmware ZIP inspection against an exported offer, including member hashes, manifest checks, container bounds and numeric version comparison.
- Read the headset's firmware version automatically and display it on the Updates page, with in-app refresh.
- Play speaking-while-muted audio alerts directly through the selected headset instead of relying on desktop notification sound hints.
- Fixed reset/reconnect selection and delayed access setup until USB permissions settle, avoiding application restarts.
- Added Linux desktop mute notifications and speech-triggered interval controls using captured headset events, with per-USB-port preferences.
- Added the IMPACT 860T ANC's seven voice-prompt language choices with automatic reads and verified setting readback.
- Kept settings dropdowns tied to their original feature while scrolling, preventing recycled rows from targeting another control.
- Added Auto Pause, tone/voice prompt controls and a confirmed Factory Reset action with fresh readback and no automatic reset retries.
- Added audio limiter, answer-on-wear, mute-on-removal and Smart Pause controls with full 16-setting readback verification.
- Automatically offers one-time system authorization for persistent headset access, activates the udev rule and reloads settings, with no repeated prompts after cancellation.
- Added eight more headset controls: boom-arm answer/mute, boom side, in-call prompt suppression, ringtone, Busylight, Talk While Muted and buttons, with complete readback verification.
- Device settings now load automatically on opening the page or changing the selected connection, with Refresh/Retry for recovery.
- Expanded sidetone to 0–100% and added TalkThrough and Low ANC using traced Windows-adapter mappings, with explicit sidetone Apply and lossless readback.
- Added live-tested sidetone/ANC controls with asynchronous HID transport, verified readback, packet-fixture tests, and a roadmap to full Windows feature parity.
- Addressed the codebase review with robust CLI parsing, asynchronous typed discovery and HID caching, explicit device selection, accessible lazy UI, MIT licensing, Qt deployment, CI, and regression/fuzz tests.
- Imported and integrity-checked the Windows headset evidence bundle with sidetone/ANC captures and candidate protocol findings.
- Added a complete codebase review with verified defects, performance measurements, and prioritized refactoring and modernization recommendations.
- Prepared a source-only repository with standalone build instructions, excluding local plans, research, captures and vendor downloads.
- Added firmware updates to the roadmap, covering discovery, package validation, installation, progress, verification and recovery.
- Reconciled the project plan with completed SDK/C++ work, consolidated duplicate tasks, and clarified evidence-dependent settings milestones and remaining release checks.
- Added typed C++23 HID parsing and errors, bounded device-metadata reads, move-only discovery providers, unit-safe timers, hardened builds and sanitizer verification.
- Upgraded the development SDK to Qt 6.12 LTS and CMake 4.4.4, added a persistent SDK command wrapper, and corrected layout issues found by the newer QML linter.
- Modularized the UI and device backend, adopted C++23 and modern QML tooling, added display-free diagnostics/validated CLI options, and documented the architecture and Qt upgrade path.
- Expanded the settings investigation into a standalone Windows-agent capture and analysis runbook with per-setting experiments, restoration checks, and evidence-bundle requirements.
- Added read-only HID diagnostics and JSON export, recovered the bundled EPOS Connect manual, and documented the settings-protocol investigation and Windows capture workflow.
- Added a runnable Qt 6 Linux prototype with live read-only IMPACT 860T ANC discovery, Windows-inspired screens, and explicitly unavailable configuration controls.
- Reviewed Windows-client screenshots and documented the UI, settings inventory, and prototype readiness in the progress plan.
